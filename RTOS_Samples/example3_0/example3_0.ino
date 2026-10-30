// EXAMPLE 3.0 - Teaching Semaphores for Signaling

/*
  This example demonstrates how a Binary Semaphore is used to signal a task to wake up.
  - Task 1 (Producer) simulates reading a physical sensor every 3 seconds.
  - Task 2 (Consumer) freezes completely until Task 1 signals that data is ready to be processed.
*/

// Define a handle for our signaling semaphore
SemaphoreHandle_t dataReadySemaphore;

// Simulated shared data buffer
int sharedSensorData = 0;

// TASK 1: The Producer (Runs on Core 0)
void Task_Sensor_Producer(void *pvParameters) {
  int fakeReadCounter = 0;

  while (1) {
    // Simulate waiting for a physical event or time interval (e.g., 3 seconds)
    vTaskDelay(pdMS_TO_TICKS(3000));  

    fakeReadCounter += 10; // Simulate a new sensor value
    sharedSensorData = fakeReadCounter;
    
    Serial.print("[Producer] Sensor read complete! Data = ");
    Serial.println(sharedSensorData);

    // 🟢 SIGNAL: Wake up the processing task. 
    // This increments the semaphore, releasing whoever is stuck at the stop line.
    xSemaphoreGive(dataReadySemaphore); 
  }
}

// TASK 2: The Consumer (Runs on Core 0)
void Task_Data_Processor(void *pvParameters) {
  while (1) {
    Serial.println("[Consumer] Waiting for new data... I am about to freeze.");

    // 🛑 INSTRUCTION STOP: This task halts right here. 
    // It consumes 0% CPU power while waiting indefinitely (portMAX_DELAY) for the signal.
    if (xSemaphoreTake(dataReadySemaphore, portMAX_DELAY) == pdTRUE) {
      
      // The task instantly springs to life the moment Task 1 calls xSemaphoreGive()
      Serial.println("[Consumer] Signal received! Processing data now...");
      
      int processedData = sharedSensorData * 2; // Simulate heavy math or logic
      
      Serial.print("[Consumer] Done! Processed Result: ");
      Serial.println(processedData);
      Serial.println("---------------------------------------------");
    }
  }
}

void setup() {
  Serial.begin(115200);
  while(!Serial); 

  // 1. Create a Binary Semaphore for signaling
  // Unlike a Mutex (which starts "available"), a Binary Semaphore starts "empty" (0).
  // This means anyone who tries to take it first will immediately freeze.
  dataReadySemaphore = xSemaphoreCreateBinary();

  if (dataReadySemaphore != NULL) {
    Serial.println("Signaling Semaphore created successfully!");
  } else {
    Serial.println("Failed to create Semaphore!");
    while(1);
  }

  // 2. Start the tasks on Core 0
  // Note: The processor task has a HIGHER priority. 
  // The moment the signal is given, the RTOS will instantly pause the Producer 
  // and jump straight to the Consumer.
  xTaskCreatePinnedToCore(Task_Sensor_Producer, "Producer", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(Task_Data_Processor, "Processor", 4096, NULL, 2, NULL, 0);
}

void loop() {
  // Main thread is free to do other background work completely unaffected
  vTaskDelay(pdMS_TO_TICKS(1000));
}
