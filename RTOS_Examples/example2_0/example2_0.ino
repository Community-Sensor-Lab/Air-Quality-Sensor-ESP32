// EXAMPLE 2.0 - Teaching Mutexes & Semaphores

/*
  This example demonstrates how to use a Mutex Semaphore to fix race conditions.
  By locking the Serial resource, we ensure tasks don't scramble the output 
  or crash the ESP32 when accessing it from different cores.
*/

// Define a handle for our Mutex Semaphore
SemaphoreHandle_t serialMutex;

String globalMessage = "I am a string in the global scope.";

void Task_Reading_Sensors(void *pvParameters) {
  String message = "Task_Reading_Sensors: I am a local string.";

  while (1) {
    // 🛑 INSTRUCTION STOP: This task will FREEZE here if another task is currently printing.
    // It waits indefinitely (portMAX_DELAY) until the token is available.
    if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
      
      // --- CRITICAL SECTION START ---
      // Only ONE task can execute this block of code at any given moment.
      Serial.println(message);
      Serial.println("Task_Reading_Sensors: " + globalMessage);
      // --- CRITICAL SECTION END ---

      // 🟢 RELEASE: Give the token back so other waiting tasks can cross their stop lines.
      xSemaphoreGive(serialMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(100));  
  }
}

void Task_Saving_To_SD(void *pvParameters) {
  String message = "Task_Saving_To_SD: I am a local string.";

  while (1) {
    // 🛑 INSTRUCTION STOP: If Task_Reading_Sensors is printing, this task freezes right here!
    if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
      
      // --- CRITICAL SECTION START ---
      Serial.println(message);
      Serial.println("Task_Saving_To_SD: " + globalMessage);
      // --- CRITICAL SECTION END ---

      // 🟢 RELEASE: Hand the token back.
      xSemaphoreGive(serialMutex);
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void setup() {
  Serial.begin(115200);
  while(!Serial); 

  // 1. Create the Mutex before starting any tasks!
  // This creates a shared "token" that only one task can hold at a time.
  serialMutex = xSemaphoreCreateMutex();

  if (serialMutex != NULL) {
    Serial.println("Mutex created successfully!");
  } else {
    Serial.println("Failed to create Mutex! Memory issue.");
    while(1); // Halt if we can't create the mutex
  }

  // 2. Spin up the tasks on Core 0
  int sensorReadingPriority = 1; 
  int savingToSDPriority = 1;

  xTaskCreatePinnedToCore(Task_Reading_Sensors, "readSensors", 4096, NULL, sensorReadingPriority, NULL, 0);
  xTaskCreatePinnedToCore(Task_Saving_To_SD, "saveSD", 4096, NULL, savingToSDPriority, NULL, 0);
}

void loop() {
  // 🛑 INSTRUCTION STOP: Even the Main Loop on Core 1 must play by the rules!
  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    
    // --- CRITICAL SECTION START ---
    Serial.println("Main Thread (Core 1): Hello World!");
    Serial.println("Main Thread (Core 1): " + globalMessage);
    // --- CRITICAL SECTION END ---

    xSemaphoreGive(serialMutex);
  }
  
  vTaskDelay(pdMS_TO_TICKS(5000));
}
