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
#include "CSL_AQS_ESP32_V1.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <secrets.h>

// --- Hardware Pin Configuration ---
//#define SD_CS_PIN 5       // Standard ESP32 SPI Chip Select pin for SD cards. Change if your shield differs.

// --- Network Configurations ---
const char* ssid     = SSID;
const char* password = PASSWORD;
#define SAMPLING_RATE 5000

// --- Queue Configuration ---
#define WIFI_CLIENT_QUEUE_SIZE 100       // Queue capacity
#define WIFI_QUEUE_BATCH_SIZE 1          // How many reading before we start sending to server?
#define SD_QUEUE_SIZE 100                // Queue capacity
#define SD_QUEUE_BATCH_SIZE 1            // How many readings before we start writing to sd card?
#define TESTING_MODE true

QueueHandle_t wclientQueue;  
QueueHandle_t sdQueue;       // Added: Dedicated queue for background SD card logging
 
WiFiClientSecure client;

String prepareData(data &sensorData);
void readSensorsAndRTC(void *pvParameters);
String preparePayload(data &sensorData);
void writingToSD(void *pvParameters);

void setup() {
  Serial.begin(115200);
  // Initialize both FreeRTOS Queues
  wclientQueue = xQueueCreate(WIFI_CLIENT_QUEUE_SIZE, sizeof(data));
  sdQueue     = xQueueCreate(SD_QUEUE_SIZE, sizeof(data));
  
  if (wclientQueue == NULL || sdQueue == NULL) {
    Serial.println("Queue allocation failed!");
    while(1);
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  // Init global vatiables to make it available for both cores
  if(TESTING_MODE){
    Serial.println(" initializeOLED()");
  }
  else{
    initializeOLED();
  }
  
  // Synchronize time via NTP (Required for TLS validation)
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }

 

  // Set the Root CA certificate for strict validation
  client.setCACert(test_root_ca);
 
  // Spawn independent threads on Core 0
  xTaskCreatePinnedToCore( readSensorsAndRTC, "readSensors", 4096, NULL, 2, NULL, 0);
  // Note: Increased stack memory to 4096 bytes. File system libraries need room.
  xTaskCreatePinnedToCore(writingToSD, "Task_SD", 4096, NULL, 1, NULL, 0);
}

// --- Main Thread: (Runs on Core 1) ---
void loop() {
  Serial.println("Main loop running...");
  if (uxQueueMessagesWaiting(wclientQueue) >= WIFI_QUEUE_BATCH_SIZE) {
    if (WiFi.status() == WL_CONNECTED) {
      for (int i = 0; i < WIFI_QUEUE_BATCH_SIZE; i++) {
        Serial.println("Sending data...");
        data sensorData;
        // Retrieve data from payload
        if (xQueueReceive(wclientQueue, &sensorData, pdMS_TO_TICKS(5)) == pdPASS) {
          doPost(preparePayload(sensorData));
        }
      }
    } 
    else {
      Serial.println("WiFi Disconnected. Waiting to batch.");
    }
  }

  vTaskDelay(pdMS_TO_TICKS(100)); 
}
