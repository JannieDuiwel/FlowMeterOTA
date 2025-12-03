#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <time.h>
#include "thingProperties.h"
#include "arduino_secrets.h"

// ---------------- Wi-Fi credentials ----------------
const char* WIFI_SSID = SECRET_SSID;
const char* WIFI_PASS = SECRET_PASS;


// ---------------- GitHub OTA config ----------------
#define FW_VERSION   "3.0.1"
#define VERSION_URL  "https://raw.githubusercontent.com/JannieDuiwel/FlowMeterOTA/main/version.txt"
#define FW_URL       "https://raw.githubusercontent.com/JannieDuiwel/FlowMeterOTA/main/firmware.bin"


// ---------------- Flow-meter config ----------------
#define FLOW_PIN 27
#define SMOOTH_ALPHA 0.1

volatile unsigned long lastPulseTime = 0;
volatile unsigned long pulseInterval = 0;

unsigned long lastMillis = 0;
float smoothedFlowLmin = 0.0;

// Time-keeping
time_t nowTime;
struct tm timeinfo;
int lastCheckedHour = -1;

// ---------------------------------------------------
//                  INTERRUPT HANDLER
// ---------------------------------------------------
void IRAM_ATTR pulseCounter() {
  unsigned long now = millis();
  static unsigned long lastISR = 0;
  if (now - lastISR > 5) {
    if (lastPulseTime > 0) pulseInterval = now - lastPulseTime;
    lastPulseTime = now;
    lastISR = now;
  }
}

// ---------------------------------------------------
//                   OTA UPDATE
// ---------------------------------------------------
bool checkForUpdate() {
  HTTPClient http;
  Serial.println("[OTA] Checking version...");
  http.begin(VERSION_URL);
  int httpCode = http.GET();

  if (httpCode != 200) { http.end(); return false; }

  String newVer = http.getString(); newVer.trim();
  http.end();

  Serial.printf("[OTA] Current: %s | Remote: %s\n", FW_VERSION, newVer.c_str());
  if (newVer == FW_VERSION) return false;

  WiFiClientSecure client; client.setInsecure();
  HTTPClient http2;
  http2.begin(client, FW_URL);
  int ret = http2.GET();

  if (ret != HTTP_CODE_OK) { http2.end(); return false; }

  int contentLen = http2.getSize();
  WiFiClient *stream = http2.getStreamPtr();

  Serial.printf("[OTA] Downloading %d bytes...\n", contentLen);
  if (!Update.begin(contentLen)) { http2.end(); return false; }

  size_t written = Update.writeStream(*stream);
  if (written == contentLen && Update.end(true)) {
    Serial.println("[OTA] Update complete. Rebooting...");
    delay(1000);
    ESP.restart();
  } else {
    Serial.printf("[OTA] Update error: %s\n", Update.errorString());
  }

  http2.end();
  return true;
}

// ---------------------------------------------------
//                        SETUP
// ---------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Booting...");

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(250); Serial.print("."); }
  Serial.println("\nWi-Fi connected.");

  // Set NTP for local time
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Syncing time");
  while (!getLocalTime(&timeinfo)) { Serial.print("."); delay(500); }
  Serial.println(" done.");

  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseCounter, FALLING);

  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();

  Serial.println("Flow monitor ready (Δt + smoothing + OTA).");
}

// ---------------------------------------------------
//                         LOOP
// ---------------------------------------------------
void loop() {
  ArduinoCloud.update();
  unsigned long now = millis();

  // ---- Flow calculation ----
  if (now - lastMillis >= 1000) {
    lastMillis = now;
    float lpp = litersPerPulse;
    if (lpp < 0.001) lpp = 100.0;

    unsigned long intervalCopy;
    noInterrupts(); intervalCopy = pulseInterval; interrupts();

    float currentFlowLmin = 0.0;
    if (intervalCopy > 0 && (now - lastPulseTime) < 5UL * intervalCopy)
      currentFlowLmin = (lpp * 60000.0) / intervalCopy;

    smoothedFlowLmin = SMOOTH_ALPHA * currentFlowLmin +
                       (1 - SMOOTH_ALPHA) * smoothedFlowLmin;

    flowRate = smoothedFlowLmin;
    flowRateM3H = flowRate * 0.06;
    totalLiters += flowRate / 60.0;

    Serial.printf("Δt:%lu ms | Flow: %.2f L/min (%.2f m³/h)\n",
                  intervalCopy, flowRate, flowRateM3H);
  }

  // ---- OTA twice per day ----
  if (getLocalTime(&timeinfo)) {
    int currentHour = timeinfo.tm_hour;
    int currentMin  = timeinfo.tm_min;

    if ((currentHour == 0 || currentHour == 12) &&
        currentMin == 0 && currentHour != lastCheckedHour) {

      Serial.println("[OTA] Scheduled update check triggered.");
      lastCheckedHour = currentHour;
      checkForUpdate();
    }

    // Reset so next day’s check will run
    if (currentHour != 0 && currentHour != 12)
      lastCheckedHour = -1;
  }
}

// ---------------------------------------------------
//                 CLOUD CALLBACKS
// ---------------------------------------------------
void onResetTotalChange() {
  if (resetTotal) {
    totalLiters = 0;
    resetTotal = false;
    Serial.println("Total volume reset.");
  }
}

void onLitersPerPulseChange() {
  Serial.printf("litersPerPulse updated → %.3f\n", litersPerPulse);
}
