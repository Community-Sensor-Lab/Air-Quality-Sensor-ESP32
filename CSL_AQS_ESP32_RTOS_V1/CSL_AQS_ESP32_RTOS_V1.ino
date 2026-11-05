/*
   COMMUNITY SENSOR LAB - AIR QUALITY SENSOR

   Adafruit Feather ESP32 V2 + Featherwing Adalogger-SD-RTC + SCD30 OR SCD41 -CO2 + BME280 -TPRH + OLED display + SEN55 -PM2.5 VOC NOX

   The SCD30 has a minimum power consumption of 5mA and cannot be stop-started. It's set to 55s (30s nominal)
   sampling period and the featherM0 sleeps for 2 x 16 =32s, wakes and waits for data available.
   Button A toggles display on/off but must be held down for 16s max and then wait 16s to toggle again.

   Logs: sensor, co2, 3x t, 3x rh, press, battery voltage, pm1, pm 2.5, pm 4.0, pm 10, voc, nox 

   https://github.com/Community-Sensor-Lab/Air-Quality-Sensor

   RICARDO TOLEDO-CROW NGENS, ESI, ASRC, CUNY, May 2025
   Edited: Jonathan Suconota
*/
#include "CSL_AQS_ESP32_RTOS_V1.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <secrets.h>

// --- Hardware Pin Configuration ---
//#define SD_CS_PIN 5       // Standard ESP32 SPI Chip Select pin for SD cards. Change if your shield differs.

// --- Network Configurations ---
#define SAMPLING_RATE 2000

// --- Queue Configuration ---
#define WIFI_CLIENT_QUEUE_SIZE 100  // Queue capacity
#define WIFI_QUEUE_BATCH_SIZE 1     // How many payloads do we want before we start sending to the server?
#define SD_QUEUE_SIZE 100           // Queue capacity
#define SD_QUEUE_BATCH_SIZE 1       // How many payloads do we want before we start writing to the sd card?

// -- Program Configuration
#define TESTING_MODE false
#define RECONNECT_TO_WIFI true
// Make the 
// #define SD_PRESENT true

// Needed to prevent race conditions when threads in core 1 and core 0 use serial.
SemaphoreHandle_t serialMutex;

QueueHandle_t wclientQueue;
QueueHandle_t sdQueue;  // Added: Dedicated queue for background SD card logging

WiFiClientSecure client;

String prepareData(data &sensorData);
void readSensorsAndRTC(void *pvParameters);
String preparePayload(data &sensorData);
void writingToSD(void *pvParameters);

void setup() {
  Serial.begin(115200);

  // Initialize both FreeRTOS Queues
  wclientQueue = xQueueCreate(WIFI_CLIENT_QUEUE_SIZE, sizeof(data));
  sdQueue = xQueueCreate(SD_QUEUE_SIZE, sizeof(data));

  if (wclientQueue == NULL || sdQueue == NULL) {
    Serial.println("Queue allocation failed!");
    while (1);
  }

  
  // Init global vatiables to make it available for both cores
  if (TESTING_MODE) {
    Serial.println(" initializeOLED()");
  } else {
    initializeOLED(display);
  }

  provisioning();

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

   // If we have a valid provision and wifi, try it out
  if (provisionInfo.valid && provisionInfo.WiFiPresent) {
    connectToWiFi();
  }

  // Go for provisioning. ENTER HERE IF TIMED OUT OR PROVISION NOT VALID OR A PRESSED OR B PRESSED
  while (!provisionInfo.valid && provisionInfo.WiFiPresent) {  // ENTER IF A PRESSED OR PROVISION NOT VALID

    uint32_t mac_reversed = (uint32_t)(ESP.getEfuseMac() >> 24) & 0xFFFFFF;
    uint32_t mac_original = ((mac_reversed & 0xFF) << 16) | ((mac_reversed & 0xFF00)) | ((mac_reversed & 0xFF0000) >> 16);  //Shift addresses to correct positions

    mac_ssid = "csl-" + String(mac_original, HEX);
    // mac_ssid = "csl-" + String((uint32_t)(ESP.getEfuseMac() >> 24) & 0xFFFFFF, HEX);

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
    Serial.println("\nWiFi Connected!");
    //Serial.println(WiFi.localIP());
    // Synchronize time via NTP (Required for TLS validation)

    syncRTCFromNPT();
  } else {
    Serial.println("\nWiFi Connection Failed!");
  }

 

  // Set the Root CA certificate for strict validation
  client.setCACert(test_root_ca);
  Serial.println("*** Adding header to google sheet. ");
  doPost(PRE_PAYLOAD_ADD_HEADER HEADER);
  Serial.println("\n*** Done adding header to google sheet");

  // Create the mutex before starting any tasks
  serialMutex = xSemaphoreCreateMutex();
  if (serialMutex == NULL) {
    Serial.println("Serial Mutex allocation failed!");
    while (1)
      ;
  }

  // Spawn independent threads on Core 0
  xTaskCreatePinnedToCore(readSensorsAndRTC, "readSensors", 4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(writingToSD, "Task_SD", 4096, NULL, 1, NULL, 0);
}

// --- Main Thread: (Runs on Core 1) ---
void loop() {
  safePrintf("Main loop running...");
  if (uxQueueMessagesWaiting(wclientQueue) >= WIFI_QUEUE_BATCH_SIZE) {
    if (WiFi.status() == WL_CONNECTED) {
      for (int i = 0; i < WIFI_QUEUE_BATCH_SIZE; i++) {
        safePrintf("Sending data...");
        data sensorData;
        if (xQueueReceive(wclientQueue, &sensorData, pdMS_TO_TICKS(5)) == pdPASS) {
          doPost(preparePayload(sensorData));
        }
      }
    } else {
      safePrintf("WiFi Disconnected");

      if (RECONNECT_TO_WIFI) {
        for (int i = 0; i < 3; i++) {
          safePrintf("Trying to reconnect");
          connectToWiFi();

        
        }
        safePrintf("\nWiFi Continuing offline");
      }
    }
  }

  vTaskDelay(pdMS_TO_TICKS(100));
}


// -- Helper Functions --

// --- Thread-Safe Print Function ---
void safePrintf(const char *format, ...) {
  // Static buffer to save heap allocation overhead during fast printing
  static char printBuffer[256];

  if (serialMutex != NULL && xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    va_list args;
    va_start(args, format);
    vsnprintf(printBuffer, sizeof(printBuffer), format, args);
    va_end(args);

    Serial.println(printBuffer);  // Safely output the compiled string to console
    xSemaphoreGive(serialMutex);  // Instantly release for other tasks
  }
}

void syncRTCFromNPT(){
  // Synchronize time via NTP (Required for TLS validation)
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
}
