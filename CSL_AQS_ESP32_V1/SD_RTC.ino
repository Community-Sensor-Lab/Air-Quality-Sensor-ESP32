#include "CSL_AQS_ESP32_V1.h"

/* INITIALIZE SD CARD */
void initializeSD() {

  if (!SD.begin()) {  // see if the card is present and can be initialized:
    Serial.println("SD card mount failed");
    return;
  } else {
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
      Serial.println("No SD card attached");
      return;
    }

    Serial.print("Starting SD card...\t\t");

    char filename[] = "/LOG0000.TXT";  // create a new file//increase 0000
    for (uint16_t i = 0; i <= 9999; i++) {
      filename[4] = i / 1000 + '0';  //48 ascii character value of 0 //integer division by
      filename[5] = i / 100 - 10 * (i / 1000) + '0';
      filename[6] = i / 10 - 10 * (i / 100) + '0';  //integer division 100
      filename[7] = i % 10 + '0';                   //modulo 10
      if (!SD.exists(filename)) {                   // only open a new file if it doesn't exist
        logfile = SD.open(filename, FILE_WRITE);
        break;  // leave the loop
      }
    }
    if (!logfile) {
      Serial.println("Couldn't create file");
    } else {
      Serial.printf("Logging to file: %s\n",filename);
      // Serial.println(filename);
      // Serial.println();
      delay(200);
      // display.print("logging to file: ");
      // display.println(filename);
      // display.display();
    }
  }
  delay(1000);
}

/* INITIALIZE RTC */
void initializeRTC() {

  Serial.print("Starting RTC...\t\t\t");
  Wire.begin();  // connect to RTC
  if (!rtc.begin()) {
    Serial.println("RTC failed");
    display.println("RTC failed");
    display.display();
  } else {
    Serial.println("RTC connected");
    display.println("RTC connected");
    
    DateTime now= rtc.now(); //UPDATE JULY 29

    char tstring[128];
    sprintf(tstring, "%02u/%02u/%02u %02u:%02u:%02u", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
    Serial.printf("Date time...\t\t\t%s\n",tstring);
    display.println(tstring);
    display.display();
  }
   //TO SET TIME at compile : run once to syncro then run again with line commented out
   //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  
}

void syncRTCFromNTP() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Skipping RTC sync: WiFi not connected");
    return;
  }

  Serial.println("Syncing RTC from NTP...");

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10000)) {
    Serial.println("RTC sync failed: no NTP time received");
    return;
  }

  DateTime ntpTime(
    timeinfo.tm_year + 1900,
    timeinfo.tm_mon + 1,
    timeinfo.tm_mday,
    timeinfo.tm_hour,
    timeinfo.tm_min,
    timeinfo.tm_sec
  );

  DateTime rtcTime = rtc.now();

  int32_t driftSeconds = (int32_t)((int64_t)ntpTime.unixtime() - (int64_t)rtcTime.unixtime());

  Serial.printf("RTC drift: %ld seconds\n", (long)driftSeconds);

  if (driftSeconds > 5 || driftSeconds < -5) {
    rtc.adjust(ntpTime);
    Serial.println("RTC adjusted from NTP");
  } else {
    Serial.println("RTC already close enough");
  }
}
