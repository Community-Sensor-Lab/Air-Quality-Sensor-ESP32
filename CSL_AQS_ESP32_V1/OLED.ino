#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>  // OLED library
#include "CSL_AQS_ESP32_V1.h"

void initializeOLED() {
  Serial.print("Starting OLED...\t\t");
  if (!display.begin(0x3C, true))  // Address 0x3C for 128x32
    Serial.println(F("SH110X  allocation failed"));
  else {
    Serial.println("OLED ok");
    display.display();
    display.setRotation(1);
    //    Serial.println("Button test");
    pinMode(BUTTON_A, INPUT_PULLUP);
    // text display tests
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    //display.println("Hello World");
    display.display();
  }

  // Set Interrupt on button A to force provisioning
  pinMode(BUTTON_A, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_A), buttonA, CHANGE);

  pinMode(BUTTON_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_B), buttonB, CHANGE);

  pinMode(BUTTON_C, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_C), buttonC, FALLING);
}

//Interrupt Handlers
void buttonA() {
  provisionInfo.valid = false;
}

void buttonB() {
  provisionInfo.WiFiPresent = false;
}

void buttonC() {
  buttonCPressed = true;
}

void handleButtonC() {
  if (!buttonCPressed) return;

  buttonCPressed = false;

  if (millis() - lastButtonCHandledMs < BUTTON_DEBOUNCE_MS) return;
  lastButtonCHandledMs = millis();

  sampleModeIndex = (sampleModeIndex + 1) % 3;
  applySampleMode();

  Serial.printf("[MODE] %s every %lu seconds\n", sampleModeText.c_str(), sampleIntervalMs / 1000);
  displaySensorStatus();
}

void applySampleMode() {
  if (sampleModeIndex == 0) {
    sampleModeText = "STATIONARY";
    sampleIntervalMs = 60000UL;
  } else if (sampleModeIndex == 1) {
    sampleModeText = "MOBILE";
    sampleIntervalMs = 30000UL;
  } else {
    sampleModeText = "FOCUS";
    sampleIntervalMs = 20000UL;
  }

  wifiUploadIntervalMs = sampleIntervalMs;
  firstWifiUpload = true;
}

// Draw the normal field screen with sensor, network, and upload status
void displaySensorStatus() {
  display.clearDisplay();
  display.setCursor(0, 0);

  display.printf("T:%.1f P:%.0f\n", sensorData.Tbme, sensorData.Pbme);
  display.printf("RH:%.0f CO2:%d\n", sensorData.RHbme, sensorData.CO2);
  display.printf("PM25:%.1f VOC:%.1f\n", sensorData.mPm2_5, sensorData.VOCs);
  display.printf("Bat:%.2fV\n", sensorData.Vbat);

  display.printf("STA:%s %d\n", staIpText.c_str(), lastWifiRssi);
  display.printf("AP:%s\n", apActive ? apIpText.c_str() : "off");
  display.printf("%s %lus\n", googleStatusText.c_str(), sampleIntervalMs / 1000);
  display.printf("%s M:%s\n", sampleModeText.c_str(), staMacShort.c_str());

  display.display();
}
