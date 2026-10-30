#include "CSL_AQS_ESP32_RTOS_V1.h"
/* BME280 TEMP, PRESS, RH BOARD FROM ADAFRUIT USING 'Adafruit_BME280.h' */

void initializeBME(Adafruit_BME280& bme280) {
  Serial.print("Starting BME280 TPRH...\t\t");
  // You can also pass in a Wire library object like &Wire2
  // bmeStatus = bme.begin(0x76, &Wire2)
  if (!bme280.begin()) {
    safePrintf("BME280 not detected");
    //display.println("BME280 not detected");
    //display.display();
  } else {
    safePrintf("BME280 connected");
    //display.println("BME280 connected");
    //display.display();
  }
}

void readBME(Adafruit_BME280& bme280, data &reading) {
  reading.Tbme  = bme280.readTemperature();
  reading.Pbme  = bme280.readPressure() / 100;  // for hPa
  reading.RHbme = bme280.readHumidity();
}

/* SCD41 CO2 SENSOR FROM SENSIRION USING "SparkFun_SCD4x_Arduino_Library.h"*/

void initializeSCD41(SCD4x& scd41) {
  Serial.print("Starting SCD41 CO2...\t\t");
  //mySensor.enableDebugging(); // Uncomment this line to get helpful debug messages on Serial
  if (scd41.begin(false, false, false) == false)  // measBegin, autoCal, skipStopPeriodicMeasurements i.e. Do not start periodic measurements
  {
    safePrintf("SCD41 sensor not detected");
    //display.println("SCD41 sensor not detected");
    //display.display();
  }
  if (!scd41.measureSingleShot()) {
    safePrintf("SCD41 measureSingleShot failed");
   // display.println("SCD41 measureSingleShot failed");
   // display.display();
  } else
    safePrintf("SCD41 connected");
  //display.println("SCD41 connected");
  //display.display();
}

void readSCD41(SCD4x& scd41, data &reading) {
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

/* SEN55 PM VOC NOX SENSOR FROM SENSIRION USING "SensirionI2CSen5x.h" */

#define MAXBUF_REQUIREMENT 48
#if ((defined(I2C_BUFFER_LENGTH) && (I2C_BUFFER_LENGTH >= MAXBUF_REQUIREMENT)) || (defined(BUFFER_LENGTH) && BUFFER_LENGTH >= MAXBUF_REQUIREMENT))
#define USE_PRODUCT_INFO
#endif

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

void readSEN55(SensirionI2CSen5x& sen55,data &reading) {
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
            reading.NOx );

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



// --- Helper Function to Generate a Random Float (0.0 to 3.3) ---
float getRandomVoltage() {
  uint32_t raw_rand = esp_random();
  return ((float)raw_rand / (float)UINT32_MAX) * 3.3f;
}

void readSensorsAndRTC(void *pvParameters){
  safePrintf("Calling Read SensorAndRTC");
  //--- Initialize RTC ----

  RTC_PCF8523 rtc; 
 // initializeRTC(&rtc); 
  Serial.print("Starting RTC...\t\t\t");
  
  Wire.begin();  // connect to RTC
  if (!rtc.begin()) {
    safePrintf("RTC failed");
    //display.println("RTC failed");
    //display.display();
  } else {
    safePrintf("RTC connected");
    //display.println("RTC connected");
    
    DateTime now= rtc.now(); //UPDATE JULY 29

    char tstring[128];
    sprintf(tstring, "%02u/%02u/%02u %02u:%02u:%02u", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
    safePrintf("Date time...\t\t\t%s\n",tstring);
    //display.println(tstring);
    //display.display();
  }
  //TO SET TIME at compile : run once to syncro then run again with line commented out
  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

 
  DateTime now;

  if(TESTING_MODE){
    safePrintf("\ninitializeBME();\ninitializeSCD41();\ninitializeSEN55();\ninitializeRTC()\n");
    
    while(1) {
      safePrintf("Sensor Task running...");

      // Datetime
      data sensorData;
      now = rtc.now();
      snprintf(sensorData.now, sizeof(sensorData.now), "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
      //readBME
      sensorData.Tbme = getRandomVoltage();
      sensorData.Pbme = getRandomVoltage();
      sensorData.RHbme = getRandomVoltage();
      //readSEN55
      sensorData.CO2 = getRandomVoltage();
      sensorData.Tco2 = getRandomVoltage();
      //readSCD41
      sensorData.mPm1_0 = getRandomVoltage();
      sensorData.mPm2_5 = getRandomVoltage();
      sensorData.mPm4_0 = getRandomVoltage();
      sensorData.mPm10 = getRandomVoltage();
      sensorData.cPm0_5 = getRandomVoltage();
      sensorData.cPm1_0 = getRandomVoltage();
      sensorData.cPm2_5 = getRandomVoltage();
      sensorData.cPm4_0 = getRandomVoltage();
      sensorData.cPm10 = getRandomVoltage();
      sensorData.tpSize = getRandomVoltage();
      sensorData.RHbme = getRandomVoltage();
      sensorData.Tsen = getRandomVoltage();
      sensorData.VOCs = getRandomVoltage();
      sensorData.NOx = getRandomVoltage();
      
      if (xQueueSend(wclientQueue, & sensorData, pdMS_TO_TICKS(0)) != pdPASS) safePrintf("wifi client queue full! Dropping sample");
      
      if (xQueueSend(sdQueue, & sensorData, pdMS_TO_TICKS(0)) != pdPASS)  safePrintf("sd queue full! Dropping sample");
      
       vTaskDelay(pdMS_TO_TICKS(2000)); 
    }

  }
  else{
    
    Adafruit_BME280 bme280;
    SCD4x scd41(SCD4x_SENSOR_SCD41);
    SensirionI2CSen5x sen55;
    initializeBME(bme280);
    initializeSEN55(sen55);
    initializeSCD41(scd41);

    while(1) {
      data sensorData;
      now = rtc.now();
      snprintf(sensorData.now, sizeof(sensorData.now), "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
      readBME(bme280,sensorData);
      readSEN55(sen55,sensorData);
      readSCD41(scd41,sensorData);

      if (xQueueSend(wclientQueue, &sensorData, pdMS_TO_TICKS(0)) != pdPASS) safePrintf("wifi client queue full! Dropping sample");
      if (xQueueSend(sdQueue, & sensorData, pdMS_TO_TICKS(0)) != pdPASS) safePrintf("sd queue full! Dropping sample");
    
      vTaskDelay(pdMS_TO_TICKS(2000)); 
    }

  }
}


void syncRTCFromNTP(RTC_PCF8523 &rtc) {
  if (WiFi.status() != WL_CONNECTED) {
    safePrintf("Skipping RTC sync: WiFi not connected");
    return;
  }

  safePrintf("Syncing RTC from NTP...");

  //configTime(-5 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  configTime(-5 * 3600, 3600, "pool.ntp.org", "time.nist.gov"); // added daylight savings

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
    timeinfo.tm_sec
  );

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
