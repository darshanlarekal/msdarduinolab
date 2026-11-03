#include <Arduino.h>

#include "calibration.h"
#include "color_data.h"
#include "config.h"
#include "detection.h"
#include "menu.h"
#include "modes.h"
#include "pick_place.h"
#include "servo_arm.h"
#include "wifi_control.h"

void setup() {
  Serial.begin(115200);

  pinMode(LDR_PIN, INPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  turnOffRGB();
  loadOldValues();
  initServo();
  initPickAndPlace();

  Serial.println("RGB + LDR CALIBRATION + COLOR DETECTOR");
  Serial.println("Arduino Uno R4 WiFi");
  Serial.println("Sensing averages 50 LDR samples per light.");

  printMenuPrompt();
  wifiControlBegin();
}

void loop() {
  wifiControlPoll();

  if (mode == MODE_MENU) {
    handleMenuInput();
  }
  else if (mode == MODE_CALIBRATION) {
    handleCalibrationInput();
  }
  else {
    runDetection();
  }
}
