/*
   COMMUNITY SENSOR LAB - AIR QUALITY SENSOR

   Adafruit Feather ESP32 V2 + Featherwing Adalogger-SD-RTC + SCD30 OR SCD41 -CO2 + BME280 -TPRH + OLED display + SEN55 -PM2.5 VOC NOX

   The SCD30 has a minimum power consumption of 5mA and cannot be stop-started. It's set to 55s (30s nominal)
   sampling period and the featherM0 sleeps for 2 x 16 =32s, wakes and waits for data available.
   Button A toggles display on/off but must be held down for 16s max and then wait 16s to toggle again.

   Logs: sensor, co2, 3x t, 3x rh, press, battery voltage, pm1, pm 2.5, pm 4.0, pm 10, voc, nox 

   https://github.com/Community-Sensor-Lab/Air-Quality-Sensor

   RICARDO TOLEDO-CROW NGENS, ESI, ASRC, CUNY, May 2025

*/
#include "CSL_AQS_ESP32_V1.h"

int currPeriod = 60000; // 2 Seconds default 

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

  provisioningFromEEPROM(); // get EEPROM info
  
  Serial.printf("10s to decide\n");
  unsigned long ts = millis();
  while(millis()-ts < WIFI_TIMEOUT && provisionInfo.valid && provisionInfo.WiFiPresent) {
    delay(500);
    Serial.print("*");
  }
  Serial.println();

  while(WiFi.status() != WL_CONNECTED && provisionInfo.WiFiPresent) {
    // get mac address 
    mac_ssid = "csl-" + String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX);

    if(!provisionInfo.valid) {
      Serial.println("going to softAPprovision");
      softAPprovision(); // may change to not valid
    } 
    if (provisionInfo.valid) {  // and we have good ssid/pw
      connectToWiFi();
    }
    if(!provisionInfo.WiFiPresent){
      return;
    }
  }

  if(!provisionInfo.WiFiPresent)
    Serial.println("No WiFi present. Continuing without WiFi.");
  
  if (WiFi.status() == WL_CONNECTED) {
    initializeClient();
    Serial.println("*** Adding header to google sheet. ");
    doPost(PRE_PAYLOAD_ADD_HEADER HEADER);
    Serial.println("\n*** Done adding header to google sheet");
    delay(5000);
  }
}

void loop() {

  String bme = readBME();
  String sen55 = readSEN55();
  String scd41 = readSCD41();
  DateTime now = rtc.now();     // fetch the date + time
  
  String rssi_quality;          //intializes wifi quality variable
  int wifi_rssi = WiFi.RSSI();  //variable for the rssi strength

  if (wifi_rssi > -50) rssi_quality = "Excellent"; 
  else if (wifi_rssi > -60) rssi_quality = "Good"; 
  else if (wifi_rssi > -70) rssi_quality = "Fair"; 
  else rssi_quality = "Poor";

  pinMode(VBATPIN, INPUT);  // read battery voltage
  sensorData.Vbat = float(analogReadMilliVolts(VBATPIN) * 2.0 / 1000.00);
  pinMode(BUTTON_A, INPUT_PULLUP);

  char tstring[128];
  sprintf(tstring, "%02u/%02u/%02u %02u:%02u:%02u, ", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());

  String outString = String(tstring) + bme + scd41 + sen55 + String(sensorData.Vbat) + "," + mac_ssid + "," + String(provisionInfo.ssid) + "," + wifi_rssi + "," + rssi_quality;  //adds all the clumns values

  Serial.println(HEADER);
  Serial.println(outString);

  logfile.println(outString);
  logfile.flush();

  display.clearDisplay();
  display.setCursor(0, 0);
  display.printf("Temp: %.2f C\nP: %.2f mBar\nRH: %.2f%%\n", sensorData.Tbme, sensorData.Pbme, sensorData.RHbme);
  display.printf("CO2: %d ppm\nPM2.5: %.2f ug/m^3\nVOCs: %.2f\n", sensorData.CO2, sensorData.mPm2_5, sensorData.VOCs);
  display.printf("Bat: %.2f V\n", sensorData.Vbat);
  display.display();

  if(WiFi.status() == WL_CONNECTED) {
    doPost(PRE_PAYLOAD_APPEND_ROW + outString);
  }
  if (!provisionInfo.valid && provisionInfo.WiFiPresent) {
    softAPprovision();
    connectToWiFi();
  }
  if (!provisionInfo.WiFiPresent) {
    Serial.println("No WiFi");
    display.println("No WiFi");
    display.display();
  } 
  
  delay(currPeriod); 
}
