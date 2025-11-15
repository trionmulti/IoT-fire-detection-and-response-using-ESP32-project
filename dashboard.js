/* dashboard.js
   - Real-time dashboard + Chart.js + alert sounds
   - Uses Firebase v10 modular SDK (CDN imports)
*/

/* ---------------- Firebase config ----------------
   Replace the firebaseConfig object values with your project's values
   from Firebase Console -> Project settings -> SDK config (Web)
*/
import { initializeApp } from "https://www.gstatic.com/firebasejs/10.12.2/firebase-app.js";
import { getDatabase, ref, onValue } from "https://www.gstatic.com/firebasejs/10.12.2/firebase-database.js";

const firebaseConfig = {
  apiKey: "AIzaSyB___vYj8mW9F-U_gIc3Y3oT7t3P4UzqTI",
  authDomain: "firedetectionsystem-d9c80.firebaseapp.com",
  databaseURL: "https://firedetectionsystem-d9c80-default-rtdb.firebaseio.com",
  projectId: "firedetectionsystem-d9c80",
  storageBucket: "firedetectionsystem-d9c80.firebasestorage.app",
  messagingSenderId: "256849534995",
  appId: "1:256849534995:web:5627419fb7fdc5151f5fe6"
};

const app = initializeApp(firebaseConfig);
const db = getDatabase(app);

/* ---------------- UI refs ---------------- */
const elTemp = document.getElementById('temp');
const elSmoke = document.getElementById('smoke');
const elFlame = document.getElementById('flame');
const elStatusText = document.getElementById('status-text');
const elLast = document.getElementById('last-update');
const elUID = document.getElementById('uid');
const overallStatusCard = document.getElementById('overall-status');

const snoozeBtn = document.getElementById('snooze');
const testBtn = document.getElementById('testAlert');

/* ---------------- Chart setup ---------------- */
const ctx = document.getElementById('realtime-chart').getContext('2d');
const maxPoints = 20;
const chartData = {
  labels: [],
  datasets: [
    { label: 'Temperature (°C)', data: [], borderColor: '#ff6b6b', backgroundColor: 'rgba(255,107,107,0.12)', tension: 0.25 },
    { label: 'Smoke', data: [], borderColor: '#ffd166', backgroundColor: 'rgba(255,209,102,0.08)', tension: 0.25 }
  ]
};
const chart = new Chart(ctx, {
  type: 'line',
  data: chartData,
  options: {
    responsive: true,
    scales: {
      x: { ticks: { color: '#cbd5e1' } },
      y: { ticks: { color: '#cbd5e1' } }
    },
    plugins: { legend: { labels: { color: '#e6eef8' } } }
  }
});

/* ---------------- Alert sound (Web Audio) ---------------- */
const audioCtx = new (window.AudioContext || window.webkitAudioContext)();
let alarmNode = null;
let alarmPlaying = false;

function playAlarm() {
  if (alarmPlaying) return;
  alarmNode = audioCtx.createOscillator();
  const gain = audioCtx.createGain();
  alarmNode.type = 'sine';
  alarmNode.frequency.value = 880; // high tone
  gain.gain.value = 0.0015;
  alarmNode.connect(gain).connect(audioCtx.destination);
  alarmNode.start();
  alarmPlaying = true;
  // ramp up volume
  gain.gain.linearRampToValueAtTime(0.12, audioCtx.currentTime + 0.2);
}

function stopAlarm() {
  if (!alarmPlaying) return;
  try {
    alarmNode.stop();
  } catch (e) {}
  alarmNode = null;
  alarmPlaying = false;
}

/* ---------------- Snooze behavior ---------------- */
let snoozedUntil = 0;
snoozeBtn.addEventListener('click', () => {
  // Snooze for 2 minutes
  snoozedUntil = Date.now() + (2 * 60 * 1000);
  snoozeBtn.textContent = 'Snoozed (2m)';
  setTimeout(() => snoozeBtn.textContent = 'Snooze Alarm', 2000);
});

/* ---------------- Local test button ---------------- */
testBtn.addEventListener('click', () => {
  handleIncomingReading({ temp: 60, smoke: 3200, flame: 0, status: 'FIRE', ts: Math.floor(Date.now()/1000), uid: 'LOCAL_TEST' });
});

/* ---------------- DB path and listener ---------------- */
// matches your ESP32 writes: /fire_system/readings/<pushId>
const readingsRef = ref(db, 'fire_system/readings');

let lastUpdateTs = 0;

onValue(readingsRef, (snap) => {
  if (!snap.exists()) return;
  const readings = snap.val();
  const keys = Object.keys(readings);
  const lastKey = keys[keys.length - 1];
  const data = readings[lastKey];
  // forward to handler
  handleIncomingReading(data);
});

/* ---------------- Handler: update UI, chart, alerts ---------------- */
function handleIncomingReading(data) {
  if (!data) return;
  // map your fieldnames
  const temp = Number(data.temp ?? 0);
  const smoke = Number(data.smoke ?? 0);
  const flame = data.flame ?? 0;
  const status = String(data.status ?? '').toUpperCase();
  const tsSec = Number(data.ts ?? Math.floor(Date.now()/1000));
  const uid = data.uid ?? '--';

  lastUpdateTs = Date.now();

  // Update metrics UI
  elTemp.textContent = (isNaN(temp) ? '--' : temp.toFixed(2));
  elSmoke.textContent = (isNaN(smoke) ? '--' : smoke);
  elFlame.textContent = flame;
  elUID.textContent = uid;
  elLast.textContent = 'Last: ' + new Date(tsSec * 1000).toLocaleString();

  // Status visual
  if (status === 'NORMAL') {
    elStatusText.textContent = 'NORMAL';
    overallStatusCard.classList.remove('blink');
    overallStatusCard.style.background = 'linear-gradient(90deg,#0f1724,#071028)';
    overallStatusCard.style.color = '#9ae6b4';
    stopAlarm();
  } else if (status === 'SMOKE') {
    elStatusText.textContent = 'SMOKE';
    overallStatusCard.classList.add('blink');
    overallStatusCard.style.background = 'linear-gradient(90deg,#45260a,#2a1306)';
    overallStatusCard.style.color = '#ffd166';
    if (Date.now() > snoozedUntil) playAlarm();
  } else if (status === 'FIRE' || status.includes('FIRE')) {
    elStatusText.textContent = 'FIRE';
    overallStatusCard.classList.add('blink');
    overallStatusCard.style.background = 'linear-gradient(90deg,#4b0b0b,#2b0606)';
    overallStatusCard.style.color = '#ff9b9b';
    if (Date.now() > snoozedUntil) playAlarm();
    // trigger a server-side alert (Cloud Function will handle send)
    // The Cloud Function triggers on DB write; nothing needed here.
  } else {
    elStatusText.textContent = status;
  }

  // Update chart (time label)
  const timeLabel = new Date(tsSec * 1000).toLocaleTimeString();
  chartData.labels.push(timeLabel);
  chartData.datasets[0].data.push(temp);
  chartData.datasets[1].data.push(smoke);

  // keep max points
  while (chartData.labels.length > maxPoints) {
    chartData.labels.shift();
    chartData.datasets.forEach(ds => ds.data.shift());
  }
  chart.update();
}

/* ---------------- Offline detection ---------------- */
setInterval(() => {
  if (Date.now() - lastUpdateTs > 90_000) { // 90s
    elStatusText.textContent = 'OFFLINE';
    overallStatusCard.style.background = 'linear-gradient(90deg,#111827,#071028)';
    overallStatusCard.style.color = '#94a3b8';
    stopAlarm();
  }
}, 5000);