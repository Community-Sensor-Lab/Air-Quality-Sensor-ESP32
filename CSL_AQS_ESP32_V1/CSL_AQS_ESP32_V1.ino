/*
   COMMUNITY SENSOR LAB - AIR QUALITY SENSOR

   Adafruit Feather ESP32 V2 + Featherwing Adalogger-SD-RTC + SCD30 OR SCD41 -CO2 + BME280 -TPRH + OLED display + SEN55 -PM2.5 VOC NOX

   The SCD30 has a minimum power consumption of 5mA and cannot be stop-started. It's set to 55s (30s nominal)
   sampling period and the featherM0 sleeps for 2 x 16 =32s, wakes and waits for data available.
   Button A toggles display on/off but must be held down for 16s max and then wait 16s to toggle again.

   Logs: sensor, co2, 3x t, 3x rh, press, battery voltage, pm1, pm 2.5, pm 4.0, pm 10, voc, nox 

   https://github.com/Community-Sensor-Lab/Air-Quality-Sensor

   RICARDO TOLEDO-CROW NGENS, ESI, ASRC, CUNY, May 2025
//Mac Address Testing
*/
#include "CSL_AQS_ESP32_V1.h"

void setup() {

  Serial.begin(115200);
  delay(5000);
  Serial.println(__FILE__);

  initializeSD();     // initializeSD has to come before initializeOLED or it'll crash
  initializeOLED();   // display
  initializeSEN55();  // PM VOC NOX sensor
  initializeSCD41();  // CO2
  //initializeSCD30(25);       // CO2 sensor to 30s more stable (1 min max recommended)
  initializeBME();  // TPRH
  initializeRTC();  // clock
  //delay(8000);

  logfile.println(HEADER);
  logfile.flush();

  Serial.printf("To force provisioning press button A\n");
  Serial.printf("To continue without WiFi press button B\n");
  display.printf("Provisioning: bttn A\n");
  display.printf("No WiFi: bttn B\n");
  display.display();

  provisioningFromEEPROM();  // get EEPROM info
  uint32_t mac_reversed = (uint32_t)(ESP.getEfuseMac() >> 24) & 0xFFFFFF;
  uint32_t mac_original = ((mac_reversed & 0xFF) << 16) | ((mac_reversed & 0xFF00)) | ((mac_reversed & 0xFF0000) >> 16);
  mac_ssid = "csl-" + String(mac_original, HEX);

  Serial.printf("10s to decide\n");
  unsigned long ts = millis();

  if (provisionInfo.valid) {  // ENTER IF PROVISION VALID, EXIT IF EITHER A OR B PRESSED OR TIMEOUT. IF EEPROM READ IS INVALID SKIPS
    int i = 10;

    while (millis() - ts < WIFI_TIMEOUT && provisionInfo.valid && provisionInfo.WiFiPresent) {
      Serial.printf("%d ", i--);
      delay(1000);
    }
    Serial.println();
  }

  // Start phone AP/status server before joining router WiFi with saved credentials
  if (provisionInfo.valid && provisionInfo.WiFiPresent) {
    softAPprovision();
    connectToWiFi();
  }

  // Go for provisioning. ENTER HERE IF TIMED OUT OR PROVISION NOT VALID OR A PRESSED OR B PRESSED
  while (!provisionInfo.valid && provisionInfo.WiFiPresent) {  // ENTER IF A PRESSED OR PROVISION NOT VALID
    Serial.println("going to softAPprovision");
    softAPprovision();  // may change to not valid
    if (provisionInfo.valid && provisionInfo.WiFiPresent) {
      connectToWiFi();
    }
  }

  if (!provisionInfo.WiFiPresent) {
    Serial.println("No WiFi present. Continuing without WiFi.");
    display.printf("No WiFi\n");
    display.display();
  }
  if (WiFi.status() == WL_CONNECTED) {
    syncRTCFromNTP();

    initializeClient();
    Serial.println("[POST] Adding header to Google sheet");

    if (doPost(PRE_PAYLOAD_ADD_HEADER HEADER)) {
      Serial.println("[POST] Header upload complete");
    } else {
      Serial.println("[POST] Header upload failed");
    }
    delay(5000);
  }
}

void loop() {
  server.handleClient();

  if (!provisionInfo.valid && provisionInfo.WiFiPresent) {
    softAPprovision();
    connectToWiFi();
  }

  if (!provisionInfo.WiFiPresent) {
    Serial.println("No WiFi");
    display.println("No WiFi");
    display.display();
    return;
  }
  // Keep loop responsive between samples so the phone web page can be served
  if (!firstSample && millis() - lastSampleMs < sampleIntervalMs) {
    return;
  }

  firstSample = false;
  lastSampleMs = millis();

  String bme = readBME();
  String sen55 = readSEN55();
  String scd41 = readSCD41();
  DateTime now = rtc.now();

  int wifi_rssi = 0;
  String rssi_quality = "No WiFi";

  if (WiFi.status() == WL_CONNECTED) {
    wifi_rssi = WiFi.RSSI();
    lastWifiRssi = wifi_rssi;
    staIpText = WiFi.localIP().toString();
    staConnected = true;
    wifiStatusText = "WiFi:OK";

    if (wifi_rssi > -50) rssi_quality = "Excellent";
    else if (wifi_rssi > -60) rssi_quality = "Good";
    else if (wifi_rssi > -70) rssi_quality = "Fair";
    else rssi_quality = "Poor";
  } else {
    staConnected = false;
    wifiStatusText = "WiFi:FAIL";
  }

  pinMode(VBATPIN, INPUT);
  sensorData.Vbat = float(analogReadMilliVolts(VBATPIN) * 2.0 / 1000.00);
  pinMode(BUTTON_A, INPUT_PULLUP);

  char tstring[128];
  sprintf(tstring, "%02u/%02u/%02u %02u:%02u:%02u, ", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());

  // WiFi upload runs on its own timer so faster sampling does not force faster uploads
  bool wifiUploadDue = WiFi.status() == WL_CONNECTED && (firstWifiUpload || millis() - lastWifiUploadMs >= wifiUploadIntervalMs);
  String uploadMethodText = wifiUploadDue ? "wifi" : "sd";

  // Add placeholder location and mode fields to keep the data format ready for cellular/GNSS
  String outString = String(tstring) + bme + scd41 + sen55 + String(sensorData.Vbat) + "," + FullmacStr + "," + String(provisionInfo.ssid) + "," + wifi_rssi + "," + rssi_quality + "," + latestLatText + "," + latestLonText + "," + latestGpsAgeText + "," + latestGpsAccuracyText + "," + latestFixValidText + "," + uploadMethodText + "," + sampleModeText + "," + String(sampleIntervalMs / 1000);
  Serial.println(HEADER);
  Serial.println(outString);

  logfile.println(outString);
  logfile.flush();

  displaySensorStatus();

  if (wifiUploadDue) {
    firstWifiUpload = false;
    lastWifiUploadMs = millis();

    if (doPost(PRE_PAYLOAD_APPEND_ROW + outString)) {
      Serial.println("[POST] Row upload complete");
    } else {
      Serial.println("[POST] Row upload failed");
    }
  }

  displaySensorStatus();
}