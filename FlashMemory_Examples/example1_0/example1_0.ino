/*
 ESP32 startup counter example with Preferences library.

 This simple example demonstrates using the Preferences library to store how many times the ESP32 module has booted. 
 The Preferences library is a wrapper around the Non-volatile storage on ESP32 processor.

 created for arduino-esp32 09 Feb 2017 by Martin Sloup (Arcao)
 
 Complete project details at https://RandomNerdTutorials.com/esp32-save-data-permanently-preferences/
 Edited by Jonathan Suconota
*/

#include <Preferences.h>

Preferences preferences;

void setup() {
  Serial.begin(115200);
  Serial.println();

  // Open Preferences with my-app namespace. Each application module, library, etc
  // has to use a namespace name to prevent key name collisions. We will open storage in
  // RW-mode (second parameter has to be false).
  // Note: Namespace name is limited to 15 chars.
  preferences.begin("wifiCredentials", false);

  // Remove all preferences under the opened namespace
  //preferences.clear();

  // Or remove the counter key only
  //preferences.remove("counter");

  // Get the counter value, if the key does not exist, return a default value of 0
  // Note: Key name is limited to 15 chars.
  String SSID = preferences.getString("SSID", "");
  String PASSWORD = preferences.getString("PASSWORD", "");
  String GSID = preferences.getString("GSID", "");

    Serial.println("First check:");
  Serial.printf("Current SSID value: %s\n", SSID.c_str());
  Serial.printf("Current PASSWORD value: %s\n", PASSWORD.c_str());
  Serial.printf("Current GSID value: %s\n", GSID.c_str());




  if (SSID == "" && PASSWORD == "" && GSID == "") {
    // TODO: 
    // Create soft AP
    // Get credentials
    SSID = "WIFI_NAME";
    PASSWORD = "12345";
    GSID = "dcssdvsd21231";

    // Store the counter to the Preferences
    preferences.putString("SSID", SSID);
    preferences.putString("PASSWORD", PASSWORD);
    preferences.putString("GSID", GSID);

    // Close the Preferences
    preferences.end();

    // Wait 10 seconds
    Serial.println("Restarting in 10 seconds...");
    delay(10000);

    // Restart ESP
    //ESP.restart();
  }
  else{

      Serial.println("Second check:");
  Serial.printf("Current SSID value: %s\n", SSID.c_str());
  Serial.printf("Current PASSWORD value: %s\n", PASSWORD.c_str());
  Serial.printf("Current GSID value: %s\n", GSID.c_str());

  }
}

void loop() {
}