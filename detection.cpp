#include "detection.h"

#include <Arduino.h>

#include "color_data.h"
#include "command_input.h"
#include "config.h"
#include "menu.h"
#include "modes.h"
#include "pick_place.h"
#include "sensing.h"
#include "servo_arm.h"

int displayedColor = 0;  // WHITE
int candidateColor = -1;
int stableCount = 0;
float lastLdrReading = 0;

void startDetection(const char* source) {
  mode = MODE_DETECTION;

  // Fresh detection run: start on white, no candidate yet.
  displayedColor = 0;
  candidateColor = -1;
  stableCount = 0;
  lastLdrReading = 0;

  showColor(displayedColor);

  Serial.println();
  Serial.print("DETECTION STARTED using ");
  Serial.println(source);
  printDetectorTable();

  Serial.println("COLOR DETECTOR READY");
  Serial.println("Send c for the menu, x for the values in use.");
  Serial.println("Servo at M2: g/b/r/w = find that color");
  Serial.println("  a/d = jog, number = jump to that angle, v = scan");
  Serial.println("Lift at A0: u = up, l = lower, m = magnet, p = pick/place");
}

void runDetection() {
  // Allow leaving detection, dumping the table, or moving the
  // servo without disturbing the measurement loop.
  if (readCommand()) {
    if (isCommand("menu", 'c')) {
      turnOffRGB();
      mode = MODE_MENU;
      printMenuPrompt();
      return;
    }

    if (isCommand("values", 'x')) {
      printDetectorTable();
      return;
    }

    if (handleServoUtilityCommand()) {
      return;
    }

    if (handlePickPlaceCommand()) {
      return;
    }

    if (seekForCommand()) {
      return;
    }
  }

  // LED stays on in its current color during measurement.
  float reading = readAverageLDR();
  lastLdrReading = reading;
  int detected = classifyReading(reading);

  if (detected == candidateColor) {
    stableCount++;
  } else {
    candidateColor = detected;
    stableCount = 1;
  }

  // Change LED only after three consecutive matching scans.
  if (stableCount >= REQUIRED_STABLE_SCANS &&
      detected != displayedColor) {

    displayedColor = detected;
    showColor(displayedColor);

    Serial.print("Detected color: ");
    Serial.println(colors[displayedColor].name);
  }

  Serial.print("LDR = ");
  Serial.print(reading, 2);

  Serial.print(" | Candidate = ");
  Serial.print(colors[detected].name);

  Serial.print(" | LED display = ");
  Serial.println(colors[displayedColor].name);

  delay(SCAN_DELAY_MS);
}
