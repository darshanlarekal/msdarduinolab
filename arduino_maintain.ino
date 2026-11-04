#include <Arduino.h>

#include "calibration.h"
#include "color_data.h"
#include "config.h"
#include "detection.h"
#include "lcd_display.h"
#include "menu.h"
#include "modes.h"
#include "pick_place.h"
#include "rgb_calibration.h"
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
  initRgbCalibrationDefaults();
  initServo();
  initPickAndPlace();
  lcdBegin();
  lcdShowMessage("COLOR SORTER", "READY");

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
