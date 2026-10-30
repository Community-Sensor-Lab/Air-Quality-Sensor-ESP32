// EXAMPLE 1.0 - Cores && Tasks(Threads) 

/*
  This example demonstrates how to start tasks on different cores, 
  and illustrates the scope of global vs. local thread variables.
*/

// Global Variables - are visible to threads/tasks across all cores.
// Benefits:
// Resources and data can be easily shared among cores.
//
// Problems:
// Race conditions. A race condition occurs when two threads try to access or modify 
// the same block of memory simultaneously, resulting in unpredictable (non-deterministic) behavior.
//
// For example, if Task 1 on Core 0 tries to write to global variable 'b', and Task 2 on Core 1 
// also tries to write to 'b' at the same time, we cannot determine which task will write last.
// If system logic depends on 'b', its behavior becomes unpredictable.
//
// Ways to address race conditions include using Mutex locks.
//
// ADDITIONAL:
// Race conditions and poor resource management can also lead to deadlocks, which can cause task starvation.
// Deadlocks occur when a task depends on a resource currently locked by another task.
// For example, Task 1 holds Resource B and waits for Resource A. Task 2 holds Resource A and waits for Resource B.
// Neither will give up what they have, so neither can proceed. They are stuck forever.

String globalMessage = "I am a string in the global scope.";

void Task_Reading_Sensors(void *pvParameters) {
  // Thread variables - are local allocated on this task's stack and only visible to this task
  String message = "Task_Reading_Sensors: I am a local string.";

  while (1) {
    Serial.println(message);

    // Global variables can be accessed by any thread
    Serial.println("Task_Reading_Sensors: " + globalMessage);

    // Putting the thread into a blocked state allows lower priority tasks to run
    vTaskDelay(pdMS_TO_TICKS(5000));  
  }
}

void Task_Saving_To_SD(void *pvParameters) {
  String message = "Task_Saving_To_SD: I am a local string.";

  while (1) {
    Serial.println(message);
    Serial.println("Task_Saving_To_SD: " + globalMessage);
    
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

// Main thread (setup and loop) runs automatically on Core 1 in Arduino ESP32
void setup() {
  Serial.begin(115200);
  while(!Serial); // Wait for serial monitor to open

  // Setting up tasks to run on Core 0
  // Note: Changing priorities changes who runs first when both wake up at the same time.
  int sensorReadingPriority = 10; 
  int savingToSDPriority = 1;

  // Creating Task 1 on Core 0
  xTaskCreatePinnedToCore(
    Task_Reading_Sensors, 
    "readSensors",     // Unique text name for debugging
    4096,              // Stack size in bytes
    NULL,              // Parameters passed passed to the task
    sensorReadingPriority, 
    NULL,              // Task handle (not used here)
    0                  // Pinned to Core 0
  );

  // Creating Task 2 on Core 0
  xTaskCreatePinnedToCore(
    Task_Saving_To_SD, 
    "saveSD",          // Fixed: Changed name from "readSensors" to "saveSD"
    4096, 
    NULL, 
    savingToSDPriority, 
    NULL, 
    0                  // Pinned to Core 0
  );
}

void loop() {
  Serial.println("Main Thread (Core 1): Hello World!");
  Serial.println("Main Thread (Core 1): " + globalMessage);
  vTaskDelay(pdMS_TO_TICKS(5000));
}
