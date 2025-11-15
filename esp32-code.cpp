/* ESP32 Fire Detection -> Firebase (REST) Example
   - Anonymous sign-up via identitytoolkit (API key)
   - Push sensor readings to RTDB path: /fire_system/readings
   - Requires: ArduinoJson (v6), ESP32 core
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>

// ---- Your network & Firebase credentials ----
const char* WIFI_SSID     = "WiFi";
const char* WIFI_PASSWORD = "password";
const char* FIREBASE_API_KEY = "FIREBASE API KEY"; // from Project Settings
const char* DATABASE_URL  = "firebaseURL"; // no trailing slash

// ---- Pins and thresholds (your original values) ----
LiquidCrystal_I2C lcd(0x27, 16, 2);

const int flamePin   = 34;
const int tempPin    = 35;
const int smokePin   = 32;
const int buzzerPin  = 25;
const int redLED     = 23;
const int yellowLED  = 27;
const int greenLED   = 14;

const int smokeThreshold = 1500;
const int fireThreshold  = 2500;
const int flameThreshold = 2000;

// ---- Firebase tokens (populated at runtime) ----
String idToken = "";       // short-lived (≈1 hour)
String refreshToken = "";  // long-lived; used to refresh idToken
String localId = "";       // user uid returned by signUp

// ---- Networking client (HTTPS) ----
WiFiClientSecure wclient;

// ---- Timing / send interval ----
unsigned long lastSend = 0;
const unsigned long sendIntervalMs = 3000UL; // send every 3s

// ---- Helper prototypes ----
bool firebaseAnonymousSignUp();
bool firebaseRefreshIdToken();
bool firebasePushReading(int smokeVal, float temperature, int flameVal, const char* status);
time_t getEpochTimestamp();

void setup() {
  Serial.begin(115200);
  delay(100);
  pinMode(redLED, OUTPUT);
  pinMode(yellowLED, OUTPUT);
  pinMode(greenLED, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Booting...");
  delay(1200);
  lcd.clear();

  // Connect WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
  lcd.setCursor(0,0);
  lcd.print("WiFi ok: ");
  lcd.print(WiFi.localIP());

  // Setup time (NTP) — to get timestamps for DB
  configTime(0, 0, "pool.ntp.org", "time.google.com"); // UTC; adjust later if needed
  Serial.println("NTP init...");

  // For simplicity in examples, disable certificate verification.
  // In production, prefer root CA pinning instead of setInsecure().
  wclient.setInsecure();

  // Sign up anonymously (get idToken/refreshToken/localId)
  if (!firebaseAnonymousSignUp()) {
    Serial.println("Firebase anonymous signup failed — will retry later.");
  } else {
    Serial.println("Firebase auth OK. uid: " + localId);
  }
}

void loop() {
  int smokeVal = analogRead(smokePin);
  int tempVal  = analogRead(tempPin);
  int flameVal = analogRead(flamePin);

  float voltage = (tempVal / 4095.0) * 3.3;
  float temperature = voltage * 100.0;

  lcd.setCursor(0,0);

  const char* status = "NORMAL";
  if (smokeVal > fireThreshold || flameVal < flameThreshold) {
    digitalWrite(redLED, HIGH); digitalWrite(yellowLED, LOW); digitalWrite(greenLED, LOW);
    tone(buzzerPin, 1000);
    lcd.print("FIRE DETECTED!  ");
    status = "FIRE";
  } else if (smokeVal > smokeThreshold) {
    digitalWrite(yellowLED, HIGH); digitalWrite(redLED, LOW); digitalWrite(greenLED, LOW);
    tone(buzzerPin, 1000); delay(200); noTone(buzzerPin); delay(200);
    lcd.print("Smoke Detected   ");
    status = "SMOKE";
  } else {
    digitalWrite(yellowLED, LOW); digitalWrite(redLED, LOW);
    digitalWrite(greenLED, HIGH); delay(500); digitalWrite(greenLED, LOW); delay(500);
    noTone(buzzerPin);
    lcd.print("Temp: ");
    lcd.print(temperature,1);
    lcd.print((char)223); lcd.print("C   ");
    lcd.setCursor(0,1);
  }

  lcd.setCursor(0,1);
  lcd.print("Smoke:");
  lcd.print(smokeVal);
  lcd.print("    ");

  // Send to Firebase periodically (or you can send only on state change)
  if (millis() - lastSend > sendIntervalMs) {
    lastSend = millis();
    bool ok = firebasePushReading(smokeVal, temperature, flameVal, status);
    Serial.printf("Firebase push result: %s\n", ok ? "OK" : "FAILED");
  }
}

// ------------------- Firebase REST helpers -------------------

bool firebaseAnonymousSignUp() {
  // POST to: https://identitytoolkit.googleapis.com/v1/accounts:signUp?key=[API_KEY]
  // Body: {}  -> creates anonymous user
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient https;
  String url = String("https://identitytoolkit.googleapis.com/v1/accounts:signUp?key=") + FIREBASE_API_KEY;
  https.begin(wclient, url);
  https.addHeader("Content-Type", "application/json");

  int httpCode = https.POST("{}");
  String payload = https.getString();
  https.end();

  Serial.println("SignUp response code: " + String(httpCode));
  Serial.println(payload);

  if (httpCode == 200) {
    // parse idToken, refreshToken, localId
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
      Serial.println("JSON parse error on signUp");
      return false;
    }
    idToken = doc["idToken"].as<String>();
    refreshToken = doc["refreshToken"].as<String>();
    localId = doc["localId"].as<String>();
    return true;
  } else {
    Serial.println("SignUp failed, code: " + String(httpCode));
    return false;
  }
}

bool firebaseRefreshIdToken() {
  // POST to securetoken.googleapis.com/v1/token?key=[API_KEY]
  // Content-Type: application/x-www-form-urlencoded
  // Body: grant_type=refresh_token&refresh_token=[REFRESH_TOKEN]
  if (refreshToken.length() == 0) return firebaseAnonymousSignUp();

  HTTPClient https;
  String url = String("https://securetoken.googleapis.com/v1/token?key=") + FIREBASE_API_KEY;
  https.begin(wclient, url);
  https.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String postBody = String("grant_type=refresh_token&refresh_token=") + refreshToken;
  int httpCode = https.POST(postBody);
  String payload = https.getString();
  https.end();

  Serial.println("Refresh token response: " + String(httpCode));
  Serial.println(payload);

  if (httpCode == 200) {
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
      Serial.println("JSON parse error on refresh");
      return false;
    }
    idToken = doc["id_token"].as<String>();
    refreshToken = doc["refresh_token"].as<String>();
    // note: securetoken returns 'id_token' key, not 'idToken'
    return true;
  }
  return false;
}

bool firebasePushReading(int smokeVal, float temperature, int flameVal, const char* status) {
  if (WiFi.status() != WL_CONNECTED) return false;

  // Ensure we have a valid idToken
  if (idToken.length() == 0) {
    if (!firebaseAnonymousSignUp()) return false;
  }

  // Compose DB endpoint: POST to /fire_system/readings.json?auth=<idToken>
  String url = String(DATABASE_URL) + String("/fire_system/readings.json?auth=") + idToken;

  // JSON body
  StaticJsonDocument<256> body;
  body["smoke"] = smokeVal;
  body["temp"] = temperature;
  body["flame"] = flameVal;
  body["status"] = status;
  body["uid"] = localId;
  body["ts"] = getEpochTimestamp();

  String out;
  serializeJson(body, out);

  HTTPClient https;
  https.begin(wclient, url);
  https.addHeader("Content-Type", "application/json");
  int httpCode = https.POST(out);
  String payload = https.getString();
  https.end();

  Serial.println("Push code: " + String(httpCode));
  Serial.println("Push payload: " + payload);

  if (httpCode == 200) return true;

  // If unauthorized, try token refresh then resend
  if (httpCode == 401 || httpCode == 403) {
    Serial.println("Auth failure -> try refresh token");
    if (firebaseRefreshIdToken()) {
      // try again recursively (careful about loops)
      return firebasePushReading(smokeVal, temperature, flameVal, status);
    } else {
      Serial.println("Refresh failed, re-signing up");
      firebaseAnonymousSignUp();
    }
  }

  return false;
}

time_t getEpochTimestamp() {
  time_t now = time(nullptr);
  if (now < 100000) { // not set yet
    // fallback to millis if NTP not ready
    return (time_t)(millis() / 1000);
  }
  return now;

}
