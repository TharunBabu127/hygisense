/*
 * ==============================================================================
 * HygieSense — ESP32 IoT Restroom Hygiene Feedback & Monitoring System
 * ==============================================================================
 * Target Hardware: ESP32 NodeMCU / ESP-WROOM-32 Dev Module
 * 
 * Hardware Wiring:
 *   - Green Button (CLEAN) -> GPIO 12 to GND (Internal INPUT_PULLUP enabled)
 *   - Red Button (DIRTY)   -> GPIO 14 to GND (Internal INPUT_PULLUP enabled)
 *   - Status LED           -> GPIO 2  (Built-in Blue LED on most ESP32 boards)
 * 
 * Operational Rules:
 *   1. Clean (Green) Button:
 *      - Increments GreenCount (+1)
 *      - Resets RedCount back to 0 (serviced/sanitized)
 *      - Sets status = "optimal"
 *      - Resets threshold count
 *   2. Dirty (Red) Button:
 *      - Increments RedCount (+1)
 *      - When RedCount reaches threshold (>= 3):
 *        Sets status = "needs_attention" to trigger immediate janitorial alert
 * 
 * Communication:
 *   Uses direct HTTPS REST API to Firebase Realtime Database.
 *   NO external third-party Firebase libraries needed!
 *   Compatible with Arduino IDE, PlatformIO, and ESP-IDF Arduino core.
 * ==============================================================================
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// -----------------------------------------------------------------------------
// WiFi Configuration
// -----------------------------------------------------------------------------
const char* WIFI_SSID     = "YOUR_WIFI_NAME";      // Replace with your WiFi SSID
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";  // Replace with your WiFi Password

// -----------------------------------------------------------------------------
// Firebase Realtime Database Configuration
// -----------------------------------------------------------------------------
// Base URL for the HygieSense Realtime Database (asia-southeast1)
const char* FIREBASE_HOST = "https://hygiesense-default-rtdb.asia-southeast1.firebasedatabase.app";

// Restroom Station Identifier (Each device should have a unique ID: Restroom_1, Restroom_2, etc.)
const char* RESTROOM_ID = "Restroom_1";

// System Alert Threshold: 3 dirty votes trigger immediate janitor dispatch
const int DIRTY_ALERT_THRESHOLD = 3;

// -----------------------------------------------------------------------------
// Pin Definitions
// -----------------------------------------------------------------------------
#define PIN_CLEAN_BTN 12   // Green Button (Clean feedback & Service Verified)
#define PIN_DIRTY_BTN 14   // Red Button (Dirty feedback)
#define PIN_LED       2    // Built-in Status LED indicator

// -----------------------------------------------------------------------------
// State Variables & Debounce
// -----------------------------------------------------------------------------
unsigned long lastCleanPressTime = 0;
unsigned long lastDirtyPressTime = 0;
const unsigned long DEBOUNCE_DELAY_MS = 300; // 300ms button debounce guard

// Local cache of counts
int cachedGreenCount = 0;
int cachedRedCount   = 0;

// -----------------------------------------------------------------------------
// Helper: Blink Status LED
// -----------------------------------------------------------------------------
void blinkLed(int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(PIN_LED, HIGH);
    delay(delayMs);
    digitalWrite(PIN_LED, LOW);
    if (i < times - 1) delay(delayMs);
  }
}

// -----------------------------------------------------------------------------
// Helper: Fetch Current Restroom State from Firebase RTDB
// -----------------------------------------------------------------------------
bool fetchCurrentCounts() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure(); // Skip SSL certificate validation for embedded simplicity

  HTTPClient https;
  String url = String(FIREBASE_HOST) + "/" + String(RESTROOM_ID) + ".json";

  if (https.begin(client, url)) {
    int httpCode = https.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = https.getString();
      Serial.println("[HTTP GET] Current Node State: " + payload);

      // Parse GreenCount and RedCount (toInt automatically stops at non-digit characters)
      int greenIdx = payload.indexOf("\"GreenCount\":");
      if (greenIdx != -1) {
        cachedGreenCount = payload.substring(greenIdx + 13).toInt();
      }

      int redIdx = payload.indexOf("\"RedCount\":");
      if (redIdx != -1) {
        cachedRedCount = payload.substring(redIdx + 11).toInt();
      }

      https.end();
      return true;
    } else {
      Serial.printf("[HTTP GET] Failed, error code: %d\n", httpCode);
    }
    https.end();
  }
  return false;
}

// -----------------------------------------------------------------------------
// Helper: Send JSON PATCH to Firebase RTDB
// -----------------------------------------------------------------------------
bool sendFirebasePatch(const String& jsonPayload) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Firebase] Error: WiFi not connected");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure(); // Accept Firebase SSL handshake

  HTTPClient https;
  String url = String(FIREBASE_HOST) + "/" + String(RESTROOM_ID) + ".json";

  if (https.begin(client, url)) {
    https.addHeader("Content-Type", "application/json");
    
    // HTTP PATCH updates only the specified fields without overwriting the whole node
    int httpCode = https.PATCH(jsonPayload);

    if (httpCode == HTTP_CODE_OK || httpCode == 204) {
      Serial.printf("[Firebase PATCH] Success (%d): %s\n", httpCode, jsonPayload.c_str());
      blinkLed(2, 60);
      https.end();
      return true;
    } else {
      Serial.printf("[Firebase PATCH] HTTP Error (%d): %s\n", httpCode, https.getString().c_str());
      https.end();
      return false;
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
// Action: Clean Button Pressed
// -----------------------------------------------------------------------------
void handleCleanButton() {
  Serial.println("\n[EVENT] Green Button Pressed -> CLEAN Feedback / Service Complete!");
  
  // Refresh latest counts from server to prevent desync
  fetchCurrentCounts();

  cachedGreenCount += 1;
  cachedRedCount = 0; // Reset dirty count back to 0 upon cleaning!

  // Build JSON payload
  // Threshold reset: status is returned to "optimal"
  String payload = "{";
  payload += "\"GreenCount\":" + String(cachedGreenCount) + ",";
  payload += "\"RedCount\":0,";
  payload += "\"status\":\"optimal\",";
  payload += "\"lastCleaned\":{\".sv\":\"timestamp\"},";
  payload += "\"lastUpdated\":{\".sv\":\"timestamp\"}";
  payload += "}";

  sendFirebasePatch(payload);
}

// -----------------------------------------------------------------------------
// Action: Dirty Button Pressed
// -----------------------------------------------------------------------------
void handleDirtyButton() {
  Serial.println("\n[EVENT] Red Button Pressed -> DIRTY Feedback!");
  
  // Refresh latest counts from server
  fetchCurrentCounts();

  cachedRedCount += 1;

  // Threshold rule: when RedCount reaches or exceeds 3, status becomes "needs_attention"
  String status = (cachedRedCount >= DIRTY_ALERT_THRESHOLD) ? "needs_attention" : "optimal";

  String payload = "{";
  payload += "\"RedCount\":" + String(cachedRedCount) + ",";
  payload += "\"status\":\"" + status + "\",";
  payload += "\"lastUpdated\":{\".sv\":\"timestamp\"}";
  payload += "}";

  if (cachedRedCount >= DIRTY_ALERT_THRESHOLD) {
    Serial.printf("[ALERT] Threshold (%d) reached! Restroom %s requires sanitation.\n", DIRTY_ALERT_THRESHOLD, RESTROOM_ID);
    blinkLed(5, 50); // Fast strobe alert on board LED
  }

  sendFirebasePatch(payload);
}

// -----------------------------------------------------------------------------
// Arduino Setup
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n==================================================");
  Serial.println("   HygieSense IoT Station — Initializing...       ");
  Serial.println("==================================================");

  // Configure Button Pins with internal pullup resistors
  pinMode(PIN_CLEAN_BTN, INPUT_PULLUP);
  pinMode(PIN_DIRTY_BTN, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  // Connect to WiFi
  Serial.printf("Connecting to WiFi: %s ", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    digitalWrite(PIN_LED, !digitalRead(PIN_LED));
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(PIN_LED, HIGH);
    Serial.println("\n[WiFi] Connected successfully!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("[WiFi] RSSI Signal: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");

    // Fetch initial node state from Firebase
    fetchCurrentCounts();
    digitalWrite(PIN_LED, LOW);
  } else {
    Serial.println("\n[WiFi] Connection timed out! Check credentials.");
  }

  Serial.println("\nSystem Ready. Awaiting user button feedback...");
  Serial.println(" - Press Green Button (GPIO 12): Record Clean & Reset Dirty to 0");
  Serial.println(" - Press Red Button   (GPIO 14): Record Dirty (Threshold: 3)");
  Serial.println("--------------------------------------------------");
}

// -----------------------------------------------------------------------------
// Arduino Main Loop
// -----------------------------------------------------------------------------
void loop() {
  // Auto-reconnect WiFi if connection drops
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastReconnectAttempt = 0;
    if (millis() - lastReconnectAttempt > 10000) {
      lastReconnectAttempt = millis();
      Serial.println("[WiFi] Reconnecting...");
      WiFi.reconnect();
    }
    return;
  }

  unsigned long currentMillis = millis();

  // Read Green Button (CLEAN) — Active LOW
  if (digitalRead(PIN_CLEAN_BTN) == LOW) {
    if (currentMillis - lastCleanPressTime > DEBOUNCE_DELAY_MS) {
      lastCleanPressTime = currentMillis;
      handleCleanButton();
      // Wait until button is released to prevent repeated triggers
      while (digitalRead(PIN_CLEAN_BTN) == LOW) {
        delay(10);
      }
    }
  }

  // Read Red Button (DIRTY) — Active LOW
  if (digitalRead(PIN_DIRTY_BTN) == LOW) {
    if (currentMillis - lastDirtyPressTime > DEBOUNCE_DELAY_MS) {
      lastDirtyPressTime = currentMillis;
      handleDirtyButton();
      // Wait until button is released
      while (digitalRead(PIN_DIRTY_BTN) == LOW) {
        delay(10);
      }
    }
  }

  delay(20); // Small loop pacing
}

