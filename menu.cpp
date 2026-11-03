#include "menu.h"

#include <Arduino.h>

#include "calibration.h"
#include "color_data.h"
#include "command_input.h"
#include "detection.h"
#include "modes.h"
#include "servo_arm.h"

Mode mode = MODE_MENU;

void printMenuPrompt() {
  Serial.println();
  Serial.println("Choose how to get calibration values:");
  Serial.println("  s = sense/calibrate strips now");
  Serial.println("  o = reuse the old saved values");
  Serial.println("Servo at M2: g/b/r/w = find that color");
  Serial.println("  a = jog left, d = jog right, or type an angle to jump there");
  Serial.println("  k = show limits, v = scan and print what the LDR sees");
}

void handleMenuInput() {
  if (!readCommand()) return;

  if (isCommand("sense", 's')) {
    // Start a fresh sensing run from the old saved values.
    loadOldValues();
    stripsCalibrated = 0;

    mode = MODE_CALIBRATION;

    Serial.println();
    Serial.println("SENSE/CALIBRATE MODE");
    printPrompt();
  }
  else if (isCommand("old", 'o')) {
    loadOldValues();
    startDetection("the old saved values");
  }
  else if (handleServoUtilityCommand()) {
    // Handled above.
  }
  else if (seekForCommand()) {
    // Handled above.
  }
  else {
    Serial.println("Invalid input. Enter s (sense/calibrate), o (old values),");
    Serial.println("or green, blue, red, white for the servo.");
  }
}

void handleCalibrationInput() {
  if (!readCommand()) return;

  if (isCommand("results", 'x')) {
    printAllResults();
    return;
  }

  if (isCommand("detect", 't')) {
    startDetection("the current calibration values");
    return;
  }

  // Jog, angle jump, limits and scan all work without leaving
  // calibration, so the arm can be positioned over each strip.
  if (handleServoUtilityCommand()) {
    return;
  }

  char strip = 0;

  if (isCommand("white", 'w')) {
    strip = 'w';
  }
  else if (isCommand("blue", 'b')) {
    strip = 'b';
  }
  else if (isCommand("green", 'g')) {
    strip = 'g';
  }
  else if (isCommand("red", 'r')) {
    strip = 'r';
  }

  if (strip != 0) {
    if (stripsCalibrated >= 4) {
      Serial.println("All four strips calibrated.");
      Serial.println("Type x to print all results.");
      return;
    }

    calibrateStrip(strip);

    if (stripsCalibrated >= 4) {
      Serial.println();
      Serial.println("All four strips calibrated.");
      startDetection("the measured calibration");
    } else {
      printPrompt();
    }

    return;
  }

  Serial.println("Invalid input. Enter w, b, g, r, x, t, a/d, <, >, angle, k, or v.");
}
