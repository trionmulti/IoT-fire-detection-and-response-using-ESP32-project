# 🔥 ESP32 Real-time Fire Monitoring Dashboard

This project provides a complete, end-to-end solution for a fire detection and monitoring system. An ESP32 device reads sensor data (temperature, smoke, flame), pushes it to Firebase using the REST API, and a real-time web dashboard subscribes to that data to display live metrics, charts, and audio alerts.


## ✨ Features

* **Device-to-Cloud:** The ESP32 firmware pushes sensor data directly to the Firebase Realtime Database.
* **Secure Device Auth:** The ESP32 uses Firebase Anonymous Authentication (via the REST API) to get a secure `idToken` for writing data.
* **Real-time Dashboard:** The web dashboard (HTML/JS) subscribes to database changes and updates instantly using the Firebase v10 SDK.
* **Data Visualization:** Includes a live-updating line chart (using Chart.js) to show historical trends for temperature and smoke.
* **Audio Alerts:** The web dashboard will play an audible alarm (using the Web Audio API) if the status is "SMOKE" or "FIRE".
* **Client-Side Snooze:** A "Snooze" button on the dashboard mutes the *browser's* alarm for 2 minutes.
* **Offline Detection:** The dashboard will show an "OFFLINE" status if no new data is received for 90 seconds.

## 💻 Technology Stack

* **Hardware:** ESP32, LCD Display (I2C), Temperature Sensor (analog), Smoke Sensor (MQ series), Flame Sensor (IR).
* **Firmware (ESP32):**
    * Arduino C++ (`esp32-code.cpp`)
    * `HTTPClient` & `WiFiClientSecure` for REST API calls.
    * `ArduinoJson` (v6) for serializing and deserializing auth tokens.
* **Backend / Database:**
    * **Firebase Realtime Database:** Stores sensor data.
    * **Firebase Authentication:** Used for secure, anonymous device sign-in.
* **Frontend (Dashboard):**
    * HTML (`web-app.html`)
    * JavaScript (`dashboard.js`) with Firebase v10 SDK (modular).
    * **Tailwind CSS** (via CDN) for styling.
    * **Chart.js** (via CDN) for graphing.

## ⚙️ How It Works (Architecture)

This project has two main components that communicate *through* Firebase.

1.  **ESP32 (Data Publisher):**
    * The `esp32-code.cpp` script connects to WiFi.
    * On boot, it calls the `identitytoolkit` (Firebase Auth) REST API to sign up anonymously.
    * It receives an `idToken` (short-lived) and a `refreshToken` (long-lived).
    * Every 3 seconds, it reads the sensors.
    * It constructs a JSON payload and `POST`s it to the `/fire_system/readings.json?auth=<idToken>` endpoint.
    * If the `idToken` expires (HTTP 401 error), it automatically uses the `refreshToken` to get a new one.

2.  **Web Dashboard (Data Subscriber):**
    * The `web-app.html` file loads `dashboard.js`.
    * `dashboard.js` initializes Firebase using the standard web SDK.
    * It creates a listener (`onValue`) on the `fire_system/readings` database path.
    * When any new data is pushed by *any* ESP32, Firebase sends the latest record to the dashboard in real-time.
    * The dashboard's `handleIncomingReading` function updates the UI, plots the chart, and triggers alarms.

### Firebase Database Structure

The ESP32 creates new records under the `readings` path. The dashboard listens for changes to this entire path.

```json
{
  "fire_system": {
    "readings": {
      "-NqA..._pushId_1": {
        "flame": 3050,
        "smoke": 480,
        "status": "NORMAL",
        "temp": 24.5,
        "ts": 1731678886,
        "uid": "Lh...device_uid"
      },
      "-NqA..._pushId_2": {
        "flame": 1800,
        "smoke": 1800,
        "status": "SMOKE",
        "temp": 35.2,
        "ts": 1731678889,
        "uid": "Lh...device_uid"
      }
    }
  }
}
```

## 🚀 Setup Guide

### 1. Firebase Project Setup

1.  **Create Project:** Go to the [Firebase Console](https://console.firebase.google.com/) and create a new project.
2.  **Enable Authentication:**
    * Go to **Authentication** -> **Sign-in method**.
    * Enable the **Anonymous** sign-in provider.
3.  **Create Realtime Database:**
    * Go to **Realtime Database** -> **Create Database**.
    * Select your region (e.g., `us-central1`).
    * Start in **test mode** for now.
4.  **Set Database Rules:**
    * Go to the **Rules** tab and paste the following. This allows any authenticated user (like your ESP32) to write, and anyone (your public dashboard) to read.
    ```json
    {
      "rules": {
        "fire_system": {
          "readings": {
            ".read": true,
            ".write": "auth != null"
          }
        }
      }
    }
    ```
5.  **Get Credentials:**
    * **For the ESP32:** Go to **Project Settings (gear icon) -> General**. Copy the **Web API Key**.
    * **For the Dashboard:** In the same settings, scroll down to **Your apps**. Click the **Web** icon (`</>`) to register a new web app. Firebase will give you a `firebaseConfig` object. Copy this entire object.

### 2. ESP32 Firmware Setup (`esp32-code.cpp`)

1.  **Update Credentials:** Open `esp32-code.cpp` and fill in:
    * `WIFI_SSID`: Your WiFi name.
    * `WIFI_PASSWORD`: Your WiFi password.
    * `FIREBASE_API_KEY`: The **Web API Key** from step 5.
    * `DATABASE_URL`: Your **Realtime Database URL** (e.g., `https://my-project-default-rtdb.firebaseio.com`).
2.  **Update Pins:** Change `flamePin`, `tempPin`, `smokePin`, etc., to match your ESP32 wiring.
3.  **Libraries:** Make sure you have the `ArduinoJson` (v6) and `LiquidCrystal_I2C` libraries installed in your Arduino IDE.
4.  **Flash:** Upload the code to your ESP32. Open the Serial Monitor at `115200` baud to check for "WiFi connected" and "Firebase auth OK".

### 3. Web Dashboard Setup (`dashboard.js`)

1.  **Rename File:** Rename `dashboard.js` to `script.js` (or change the `<script>` tag in `web-app.html` to point to `dashboard.js`).
2.  **Update Config:** Open `dashboard.js` (now `script.js`) and replace the entire placeholder `firebaseConfig` object with the one you copied from the Firebase console in step 5.
    ```javascript
    // Replace this entire object
    const firebaseConfig = {
      apiKey: "AIzaSyB...",
      authDomain: "my-project.firebaseapp.com",
      databaseURL: "[https://my-project-default-rtdb.firebaseio.com](https://my-project-default-rtdb.firebaseio.com)",
      // ...etc
    };
    ```

### 4. Run the Project

1.  Power on your ESP32.
2.  Host the `web-app.html` and `script.js` files. You can:
    * Use a simple web server like **Firebase Hosting** (recommended).
    * Use a VS Code extension like **Live Server**.
    * Simply open `web-app.html` directly in your browser.

You should now see the metrics and chart update in real-time as your ESP32 sends data.

## 📄 License

This project is open-source and available under the [MIT License](LICENSE).
