#include <Arduino.h>

#include "color_data.h"
#include "config.h"
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
  initRgbCalibrationDefaults();
  initServo();
  initPickAndPlace();
  lcdBegin();
  lcdShowMessage("COLOR SORTER", "READY");

  Serial.println("RGB + LDR CALIBRATION + COLOR DETECTOR");
  Serial.println("Arduino Uno R4 WiFi");
  Serial.println("Sensing averages 20 LDR samples per reading.");

  printMenuPrompt();
  wifiControlBegin();
}

void loop() {
  wifiControlPoll();

  if (mode == MODE_MENU) {
    handleMenuInput();
  }
}
