#include "CSL_AQS_ESP32_RTOS_V1.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>  // OLED library

void initializeOLED()  {
  // Large Oled display
  Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire);

  safePrintf("Starting OLED...\t\t");
  if (!display.begin(0x3C, true)) // Address 0x3C for 128x32
    safePrintf("SH110X  allocation failed");
  else  {
    safePrintf("OLED ok");
    display.display();
    display.setRotation(1);
    //    safePrintfln("Button test");
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

}

//Interrupt Handlers
void buttonA() {
  provisionInfo.valid = false;
}

void buttonB() {
  provisionInfo.WiFiPresent = false;
}