#include "CSL_AQS_ESP32_RTOS_V1.h"
#include <vector> // Required for dynamic batch sizing

/* INITIALIZE SD CARD */
void writingToSDTask(void *pvParameters) {

  File logfile;
  char filename[] = "/LOG0000.TXT";
  bool fileCreated = false;
  
  if (!SD.begin()) {  // see if the card is present and can be initialized:
    safePrintf("SD card mount failed");
  } else {
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
      safePrintf("No SD card attached");
    } else {
      Serial.print("Starting SD card...\t\t");
      
      // Create file
      for (uint16_t i = 0; i <= 9999; i++) {
        filename[4] = i / 1000 + '0';  
        filename[5] = (i / 100) % 10 + '0'; 
        filename[6] = (i / 10) % 10 + '0';  
        filename[7] = i % 10 + '0';         
        
        if (!SD.exists(filename)) {                  // only open a new file if it doesn't exist
          logfile = SD.open(filename, FILE_WRITE);
          logfile.println(HEADER);
          logfile.flush();
          logfile.close();
          fileCreated = true;
          Serial.printf("Logging to file: %s\n", filename);
          break;  // leave the loop
        }
      }

      if (!fileCreated) {
        safePrintf("Couldn't create file");
      } else {
        bool sdConnected = true;  // Track state

        while (1) {
          // Block until the first message arrives (with a 5-second timeout)
          data firstItem;
          if (xQueueReceive(sdQueue, &firstItem, pdMS_TO_TICKS(5000)) == pdPASS) {
            
            // Check how many additional items are currently waiting in the queue
            UBaseType_t waitingCount = uxQueueMessagesWaiting(sdQueue);
            
            // Initialize a dynamic vector to hold all items (1 popped + all waiting)
            std::vector<data> batch;
            batch.reserve(1 + waitingCount);
            batch.push_back(firstItem);

            // Drain all remaining items currently in the queue without blocking
            data nextItem;
            while (sdConnected && xQueueReceive(sdQueue, &nextItem, 0) == pdPASS) {
              batch.push_back(nextItem);
            }

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
                // Write the entire dynamic batch in one file open/close cycle
                for (size_t i = 0; i < batch.size(); i++) {
                  logfile.printf("%s,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
                                 batch[i].now,
                                 batch[i].Tbme, batch[i].Pbme, batch[i].RHbme,
                                 batch[i].CO2, batch[i].Tco2, batch[i].RHco2,
                                 batch[i].mPm1_0, batch[i].mPm2_5, batch[i].mPm4_0, batch[i].mPm10, batch[i].cPm0_5, batch[i].cPm1_0, batch[i].cPm2_5,
                                 batch[i].cPm2_5, batch[i].cPm4_0, batch[i].cPm10, batch[i].tpSize, batch[i].RHsen, batch[i].Tsen, batch[i].VOCs, batch[i].NOx);
                }
                logfile.close();
                safePrintf("[SD Background Task] Wrote dynamic batch of %u readings successfully.", (unsigned int)batch.size());
              } else {
                // Write failed — card likely removed or corrupted
                safePrintf("[SD Background Task] Error: Could not open file! Marking disconnected.");
                SD.end();  // Unmount cleanly
                sdConnected = false;
              }
            } else {
              safePrintf("[SD] Card offline. Batch of %u items dropped.", (unsigned int)batch.size());
            }
          }
        }
      }
    }
  }
  safePrintf("Failed to find SD. Reset board once added.");
  vTaskDelete(NULL); 
}