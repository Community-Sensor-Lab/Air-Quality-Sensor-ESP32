#include "CSL_AQS_ESP32_RTOS_V1.h"
/* INITIALIZE SD CARD */
void writingToSD(void *pvParameters) {

  File logfile;
  char filename[] = "/LOG0000.TXT";
  bool fileCreated = false;
  if (!SD.begin()) {  // see if the card is present and can be initialized:
    safePrintf("SD card mount failed");
    //return;
  } else {
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
      safePrintf("No SD card attached");
      //return;
    } else {
      Serial.print("Starting SD card...\t\t");
      // Create file
      for (uint16_t i = 0; i <= 9999; i++) {
        filename[4] = i / 1000 + '0';  //48 ascii character value of 0 //integer division by
        filename[5] = i / 100 - 10 * (i / 1000) + '0';
        filename[6] = i / 10 - 10 * (i / 100) + '0';  //integer division 100
        filename[7] = i % 10 + '0';                   //modulo 10
        if (!SD.exists(filename)) {                   // only open a new file if it doesn't exist
          logfile = SD.open(filename, FILE_WRITE);
          logfile.println(HEADER);
          fileCreated = true;
          Serial.printf("Logging to file: %s\n", filename);
          break;  // leave the loop
        }
      }



      if (!fileCreated) {
        safePrintf("Couldn't create file");

      } else {
        data logData;
        bool sdConnected = true;  // Track state

        while (1) {
            safePrintf("SD Task running...");

          if (uxQueueMessagesWaiting(wclientQueue) >=SD_QUEUE_BATCH_SIZE) {

            for (int i = 0; i < SD_QUEUE_BATCH_SIZE; i++) {
              if (xQueueReceive(sdQueue, &logData, pdMS_TO_TICKS(1000)) == pdPASS) {
                // If previously disconnected, try to re-initialize before writing
                if (!sdConnected) {
                  safePrintf("[SD] Attempting to re-mount SD card...");
                  if (SD.begin()) {
                    sdConnected = true;
                    safePrintf("[SD] Re-connected successfully!");
                  }
                }

                if (sdConnected) {
                  logfile = SD.open(filename, FILE_APPEND);
                  if (logfile) {
                    logfile.printf("%s,%.2.2f,%.2.2f,%.2.2f,%.2.2f,%.2.2f,%.2.2f,%.2.2f,%.2.2f,%.2.2f,%.2.2f,%.2.2f,%.2.2f,%.2.2f,%.2.2f,%.2.2f,%.2.2f,%.2.2f,%.2.2f,%.2.2f,%.2.2f,%.2.2f\n",
                                   logData.now,
                                   logData.Tbme, logData.Pbme, logData.RHbme,
                                   logData.CO2, logData.Tco2, logData.RHco2,
                                   logData.mPm1_0, logData.mPm2_5, logData.mPm4_0, logData.mPm10, logData.cPm0_5, logData.cPm1_0, logData.cPm2_5,
                                   logData.cPm2_5, logData.cPm4_0, logData.cPm10, logData.tpSize, logData.RHsen, logData.Tsen, logData.VOCs, logData.NOx);
                    logfile.close();
                    safePrintf("[SD Background Task] Wrote reading successfully.");
                  } else {
                    // Write failed — card likely removed or corrupted
                    safePrintf("[SD Background Task] Error: Could not open file! Marking disconnected.");
                    SD.end();  // Unmount cleanly
                    sdConnected = false;
                  }
                } else {
                  safePrintf("[SD] Card offline. Dropping log or saving to fallback queue.");
                }
              }
            }
          }
          vTaskDelay(pdMS_TO_TICKS(2000));
        }
      }
    }
  }
  safePrintf("Failed to find SD. Reset board once added.");
  vTaskDelete(NULL); 
}