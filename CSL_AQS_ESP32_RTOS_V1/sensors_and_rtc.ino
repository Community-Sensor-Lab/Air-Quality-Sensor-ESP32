#include "CSL_AQS_ESP32_RTOS_V1.h"


/*
  =====================================================================
  Sensors and RTC 
  Initializes references to BME280, SCD41, and SEN55 sensors. 


  =====================================================================
*/


/**
* Sensor: BME280 
* Measurements: temp, press, rh 
* Board: from Adafruit
* Library:'Adafruit_BME280.h'
*
* @brief Initializes the BME280 sensor
* @param bme280 sensor reference
* @return void
*/
void initializeBME280(Adafruit_BME280& bme280) {
  Serial.print("Starting BME280 TPRH...\t\t");
  // You can also pass in a Wire library object like &Wire2
  // bmeStatus = bme.begin(0x76, &Wire2)
  if (!bme280.begin()) {
    safePrintf("BME280 not detected");
    display.println("BME280 not detected");
    display.display();
  } else {
    safePrintf("BME280 connected");
    display.println("BME280 connected");
    display.display();
  }
}

/**
* @brief Read the SCD41 sensor and store in the 'data' struct
* @param scd280 sensor reference 
* @param data   struct reference
* @return void
*/
void readBME(Adafruit_BME280& bme280, data& reading) {
  reading.Tbme = bme280.readTemperature();
  reading.Pbme = bme280.readPressure() / 100;  // for hPa
  reading.RHbme = bme280.readHumidity();
}


/**
* Sensor Details
*   Sensor: SCD41
*   Meassures: CO2
*   Board: from Sensirion
*   Library: "SparkFun_SCD4x_Arduino_Library.h"

* @brief Initializes the SCD41 sensor
* @param scd41 sensor reference
* @return void
*/
void initializeSCD41(SCD4x& scd41) {
  Serial.print("Starting SCD41 CO2...\t\t");
  //mySensor.enableDebugging(); // Uncomment this line to get helpful debug messages on Serial
  if (scd41.begin(false, false, false) == false)  // measBegin, autoCal, skipStopPeriodicMeasurements i.e. Do not start periodic measurements
  {
    safePrintf("SCD41 sensor not detected");
    display.println("SCD41 sensor not detected");
    display.display();
  }
  if (!scd41.measureSingleShot()) {
    safePrintf("SCD41 measureSingleShot failed");
    display.println("SCD41 measureSingleShot failed");
    display.display();
  } else
    safePrintf("SCD41 connected");
  display.println("SCD41 connected");
  display.display();
}

/**
* @brief Initializes the SCD41 sensor
* @param scd41 sensor reference 
* @param data  struct reference 
* @return void
*/
void readSCD41(SCD4x& scd41, data& reading) {
  scd41.measureSingleShot();        // Request fresh data (should take 5 seconds)
  while (!scd41.readMeasurement())  // readMeasurement will return true when fresh data is available
  {
    Serial.print("+");
    delay(500);
  }
  reading.CO2 = scd41.getCO2();
  reading.Tco2 = scd41.getTemperature();
  reading.RHco2 = scd41.getHumidity();
}


#define MAXBUF_REQUIREMENT 48
#if ((defined(I2C_BUFFER_LENGTH) && (I2C_BUFFER_LENGTH >= MAXBUF_REQUIREMENT)) || (defined(BUFFER_LENGTH) && BUFFER_LENGTH >= MAXBUF_REQUIREMENT))
#define USE_PRODUCT_INFO
#endif

/** 
*  Sensor: SEN55
*  Meassures: pm voc nox 
*  Board: from Sensirion
*  Library: "SensirionI2CSen5x.h" 
*
* @brief Initializes the SCD55 sensor
* @param sen55 sensor reference
* @return void
*/
void initializeSEN55(SensirionI2CSen5x& sen55) {
  Serial.print("Starting SEN55 PM2.5...\t\t");
  Wire.begin();
  sen55.begin(Wire);

  uint16_t error;
  char errorMessage[256];
  error = sen55.deviceReset();
  if (error) {
    Serial.print("Error trying to execute deviceReset(): ");
    errorToString(error, errorMessage, 256);
    safePrintf(errorMessage);
  }

  float tempOffset = 0.0;
  error = sen55.setTemperatureOffsetSimple(tempOffset);
  if (error) {
    Serial.print("Error trying to execute setTemperatureOffsetSimple(): ");
    errorToString(error, errorMessage, 256);
    safePrintf(errorMessage);
  } else {
    //safePrintf("T offset %.2f C...\t", tempOffset);
  }
  error = sen55.startMeasurement();
  if (error) {
    Serial.print("Error trying to execute startMeasurement(): ");
    errorToString(error, errorMessage, 256);
    safePrintf(errorMessage);
  } else {
    safePrintf("SEN55 connected");
    //display.println("SEN55 connected");
    //display.display();
  }
}

/**
* @brief Initializes the SCD41 sensor
* @param scd41 sensor reference 
* @param data  struct reference
* @return void
*/
void readSEN55(SensirionI2CSen5x& sen55, data& reading) {
  uint16_t error;
  char errorMessage[256];
  delay(1000);

  error = sen55.readMeasuredValues(
    reading.mPm1_0,
    reading.mPm2_5,
    reading.mPm4_0,
    reading.mPm10,
    reading.RHsen,
    reading.Tsen,
    reading.VOCs,
    reading.NOx);

  if (error) {
    Serial.print("Error trying to execute readMeasuredValues(): ");
    errorToString(error, errorMessage, 256);
    safePrintf(errorMessage);
  }

  error = sen55.readMeasuredPmValues(
    reading.mPm1_0,
    reading.mPm2_5,
    reading.mPm4_0,
    reading.mPm10,
    reading.cPm0_5,
    reading.cPm1_0,
    reading.cPm2_5,
    reading.cPm4_0,
    reading.cPm10,
    reading.tpSize);

  if (error) {
    Serial.print("Error trying to execute readMeasuredPmValues(): ");
    errorToString(error, errorMessage, 256);
    safePrintf(errorMessage);
  }
}


/**
* @brief Sets up the sensors and rtc to start the task
* @param pvParameters (optional) 
* @return void
*/
void readSensorsAndRTCTask(void* pvParameters) {
  safePrintf("Creating readSensorsAndRTC task");
  //--- Initialize RTC ----
  display.clearDisplay();
  display.setCursor(0, 0);

  RTC_PCF8523 rtc;
  Serial.print("Starting RTC...\t\t\t");

  Wire.begin();  // connect to RTC
  if (!rtc.begin()) {
    safePrintf("RTC failed");
    display.println("RTC failed");
    display.display();
  } else {
    safePrintf("RTC connected");
    display.println("RTC connected");
    DateTime now = rtc.now();
    char tstring[128];
    sprintf(tstring, "%02u/%02u/%02u %02u:%02u:%02u", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
    safePrintf("Date time...\t\t\t%s\n", tstring);
    display.println(tstring);
    display.display();
  }
  //TO SET TIME at compile : run once to syncro then run again with line commented out
  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  DateTime now;
  display.display();

  Adafruit_BME280 bme280;
  SCD4x scd41(SCD4x_SENSOR_SCD41);
  SensirionI2CSen5x sen55;
  initializeBME280(bme280);
  initializeSEN55(sen55);
  initializeSCD41(scd41);

  while (1) {
    data sensorData;
    now = rtc.now();
    snprintf(sensorData.now, sizeof(sensorData.now), "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
    readBME(bme280, sensorData);
    readSEN55(sen55, sensorData);
    readSCD41(scd41, sensorData);

    if (xQueueSend(wclientQueue, &sensorData, pdMS_TO_TICKS(0)) != pdPASS) safePrintf("wifi client queue full! Dropping sample");
    if (xQueueSend(sdQueue, &sensorData, pdMS_TO_TICKS(0)) != pdPASS) safePrintf("sd queue full! Dropping sample");

    vTaskDelay(pdMS_TO_TICKS(SAMPLING_PERIOD));
  }
}


/**
* @brief Synchronize time via NTP (Required for TLS validation)
* @param rtc real time clock reference
* @return void
*/
void syncRTCFromNTP(RTC_PCF8523& rtc) {
  if (WiFi.status() != WL_CONNECTED) {
    safePrintf("Skipping RTC sync: WiFi not connected");
    return;
  }

  safePrintf("Syncing RTC from NTP...");

  //configTime(-5 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  configTime(-5 * 3600, 3600, "pool.ntp.org", "time.nist.gov");  // added daylight savings

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10000)) {
    safePrintf("RTC sync failed: no NTP time received");
    return;
  }

  DateTime ntpTime(
    timeinfo.tm_year + 1900,
    timeinfo.tm_mon + 1,
    timeinfo.tm_mday,
    timeinfo.tm_hour,
    timeinfo.tm_min,
    timeinfo.tm_sec);

  DateTime rtcTime = rtc.now();

  int32_t driftSeconds = (int32_t)((int64_t)ntpTime.unixtime() - (int64_t)rtcTime.unixtime());

  safePrintf("RTC drift: %ld seconds\n", (long)driftSeconds);

  if (driftSeconds > 5 || driftSeconds < -5) {
    rtc.adjust(ntpTime);
    safePrintf("RTC adjusted from NTP");
  } else {
    safePrintf("RTC already close enough");
  }
}
