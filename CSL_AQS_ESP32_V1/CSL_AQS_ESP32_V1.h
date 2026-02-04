#ifndef CSL_AQI_ESP32_V1_H
#define CSL_AQI_ESP32_V1_H

#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <RTClib.h>
// #include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>  // OLED library

#define VBATPIN A13  // this is also D9 button A disable pullup to read analog
#define BUTTON_A 15  // for the adafruit Feather ESP32 v2 (ABC, 15 32 14) Oled button also A7 enable pullup to read button
#define BUTTON_B 32  // for the adafruit Feather ESP32 v2 (ABC, 15 32 14) Oled button also A7 enable pullup to read button
#define WIFI_TIMEOUT 10000 // how long to wait for connection in ms
//#define SD_CS 10    // Chip select for SD card default for Adalogger

/* STRUCT TO STORE ALL SENSOR DATA */
typedef struct {
  DateTime now; 
  float Tbme;  // BME280 temperature C
  float Pbme;  // BME280 pressure mBar
  float RHbme; // BME280 relative humidity %

  uint16_t CO2; // SCD41 C02 in ppm
  float Tco2;   // SCD41 temperature C
  float RHco2;  // SCD41 relative humidity %
  
  float mPm1_0; // SEN55 pm1.0 in ug/m^3
  float mPm2_5; // SEN55 pm2.5 in ug/m^3
  float mPm4_0; // SEN55 pm4.0 in ug/m^3
  float mPm10;  // SEN55 pm10  in ug/m^3

  float cPm0_5; // SEN55 pm0.5 in #/cm^3
  float cPm1_0; // SEN55 pm1.0 in #/cm^3
  float cPm2_5; // SEN55 pm2.5 in #/cm^3
  float cPm4_0; // SEN55 pm4.0 in #/cm^3
  float cPm10;  // SEN55 pm10  in #/cm^3
  float tpSize; // SEN55 typical particle Size

  float RHsen; // SEN55 relative humidity %
  float Tsen;  // SEN55 temperature C
  float VOCs;  // SEN55 VOCs index [1..500] 
  float NOx;   // SEN55 NOx index [1..500]

  float Vbat;
} data;

data sensorData; // instantiate a sensor data structure

// global web server
static WebServer server(80);
static String mac_ssid;

// the title of the columns
#define HEADER "DateTime, Tbme, Pbme, RHbme, CO2, Tco2, RHco2, mPm1.0, mPm2.5, mPm4.0, mPm10, cPm0.5, cPm1.0, cPm2.5, cPm4.0, cPm10, cPm tSize, RHsen, Tsen, VOCs, NOx, Vbat, ID (Mac Address), WiFi, WiFi rssi (dBm), WiFi Quality" 

// large Oled display
Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire);

// Clock and SD card
RTC_PCF8523 rtc;                                                 // Real Time Clock for RevB Adafruit logger shield
File logfile;                                                    // the logging file

// wifi and google sheets provisioning 
// typedef struct {
//   boolean valid;
//   char ssid[64];
//   char passcode[64];
//   char gsid[128];
//   boolean WiFiPresent;
// } Secrets;
// Secrets provisionInfo;

struct Secrets {
  bool  valid;
  char  ssid[64];
  char  passcode[64];
  char  gsid[128];
  bool  WiFiPresent;
};

// A versioned, checksummed record stored in EEPROM.
struct SecretsRecord {
  uint32_t magic;      // constant marker
  uint16_t version;    // increment when struct layout changes
  uint16_t length;     // sizeof(Secrets) at time of write
  uint32_t crc32;      // CRC of payload only
  Secrets  payload;
};

// constants
static constexpr uint32_t SECRETS_MAGIC   = 0x53454352; // 'SECR'
static constexpr uint16_t SECRETS_VERSION = 1;
static constexpr int      EEPROM_ADDR     = 0;

Secrets provisionInfo;

#define PRE_PAYLOAD_APPEND_ROW "{\"command\":\"appendRow\",\"sheet_name\":\"Sheet1\",\"values\":\""
#define PRE_PAYLOAD_ADD_HEADER "{\"command\":\"addHeader\",\"sheet_name\":\"Sheet1\",\"values\":\""

/* OLD CODE FOR WEBPAGE
static const char provisioningPage[] = R"===(
<!DOCTYPE HTML><html><head>
  <title>Community Sensor Lab provisioning page</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <form action="/get">
    ssid: <input type="text" name="SSID"><br>
    <!-- <input type="submit" value="Submit">
  </form><br>
  <form action="/get"> -->
    passcode: <input type="password" name="passcode"><br>
    <!-- <input type="submit" value="Submit">
   </form><br>
 <form action="/get"> -->
    gsid: <input type="text" name="GSID"><br>
    <input type="submit" value="Submit">
  </form>
</body></html>
)===";
*/
#endif

