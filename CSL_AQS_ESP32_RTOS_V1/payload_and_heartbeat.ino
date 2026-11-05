#include "CSL_AQS_ESP32_RTOS_V1.h"
#define POST_PAYLOAD "\"}"

void initializeClient() {
  client.setCACert(test_root_ca);
}

// --- Payload Upload (Runs independently on Core 0) ---
void doPost(String outstr) {
  HTTPClient http;
  // Check queue condition first

  if (WiFi.status() != WL_CONNECTED) {
    safePrintf("[Upload] Wi-Fi disconnected, skipping upload.");
    return;
  }

  http.setTimeout(10000);
  String payload = String(PRE_PAYLOAD_APPEND_ROW) + outstr + POST_PAYLOAD;

  safePrintf("\nStarting connection to server... ");

  // Initialize HTTPClient with the secure client
  String APP_SCRIPT_URL = "https://script.google.com/macros/s/"+ String(provisionInfo.gsid) +"/exec";
  if (http.begin(client, APP_SCRIPT_URL)) {
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0) {
      safePrintf("HTTP Response code: %d\n", httpResponseCode);
      String response = http.getString();
      safePrintf("Server response: ",response.c_str());
    } else {
      safePrintf("Error on sending POST: %s\n", http.errorToString(httpResponseCode).c_str());
    }
    http.end();
  } else {
    safePrintf("[Upload] Failed to initialize HTTP client connection.");
  }

  sendHeartbeat();
}

// --- Heartbeat Task (Runs independently on Core 0) ---
void sendHeartbeat() {
  HTTPClient http;
  safePrintf("Initialize Heartbeat Task");

  if (WiFi.status() != WL_CONNECTED) {
    safePrintf("[Heartbeat] Wi-Fi disconnected, skipping pulse.");
    return;
  }

  http.setTimeout(10000);
  String payload = String(PRE_PAYLOAD_APPEND_ROW) + "Alive" + POST_PAYLOAD;

  safePrintf("\nStarting connection to server... ");
  String APP_SCRIPT_URL = "https://script.google.com/macros/s/"+ String(provisionInfo.gsid) +"/exec";
  if (http.begin(client, APP_SCRIPT_URL)) {
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0) {
      safePrintf("HTTP Response code: %d\n", httpResponseCode);
      String response = http.getString();
      safePrintf("Server response: ",response.c_str());
    } else {
      safePrintf("Error on sending POST: %s\n", http.errorToString(httpResponseCode).c_str());
    }
    http.end();
  } else {
    safePrintf("[Heartbeat] Failed to initialize HTTP client connection.");
  }
}

// --- Payload preparation ---
String preparePayload(data& sensorData) {
  return (sensorData.now + String(", ")             // Datetime
          + String(sensorData.Tbme) + String(", ")  // BME
          + String(sensorData.Pbme) + String(", ")
          + String(sensorData.RHbme) + String(", ")

          + String(sensorData.CO2) + String(", ")  // SCD41
          + String(sensorData.Tco2) + String(", ")
          + String(sensorData.RHco2) + String(", ")

          + String(sensorData.mPm1_0) + String(", ")  // SEN55
          + String(sensorData.mPm2_5) + String(", ")
          + String(sensorData.mPm4_0) + String(", ")
          + String(sensorData.mPm10) + String(", ")

          + String(sensorData.cPm0_5) + String(", ")
          + String(sensorData.cPm1_0) + String(", ")
          + String(sensorData.cPm2_5) + String(", ")
          + String(sensorData.cPm4_0) + String(", ")
          + String(sensorData.cPm10) + String(", ")
          + String(sensorData.tpSize) + String(", ")

          + String(sensorData.RHsen) + String(", ")
          + String(sensorData.Tsen) + String(", ")
          + String(sensorData.VOCs) + String(", ")
          + String(sensorData.NOx));
}