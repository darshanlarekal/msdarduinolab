#include "assignment_flow.h"

#include <Arduino.h>
#include <string.h>

#include "color_data.h"
#include "command_input.h"
#include "config.h"
#include "lcd_display.h"
#include "modes.h"
#include "pick_place.h"
#include "rgb_calibration.h"
#include "wifi_control.h"

static const char* assignmentPrompt = "READY";

const char* assignmentPromptText() {
  return assignmentPrompt;
}

static int colorIndexFromToken(const char* token) {
  if (token == NULL || token[0] == '\0') return -1;

  if (token[1] == '\0') {
    return stripIndexFor(token[0]);
  }

  if (strcmp(token, "white") == 0) return 0;
  if (strcmp(token, "green") == 0) return 1;
  if (strcmp(token, "red") == 0) return 2;
  if (strcmp(token, "blue") == 0) return 3;

  return -1;
}

static int promptColor(const char* label) {
  assignmentPrompt = label;
  lcdShowMessage(label, "w/g/r/b  c=EXIT");
  Serial.println();
  Serial.print(label);
  Serial.println(" color (w/g/r/b, c to exit):");

  while (true) {
    while (!readCommand()) {
      wifiControlPoll();
      delay(20);
    }

    int index = colorIndexFromToken(currentCommand());

    if (index >= 0) return index;
    if (strcmp(currentCommand(), "c") == 0) return -1;

    Serial.println("Invalid color. Enter w, g, r, b, or c.");
    lcdShowMessage(label, "INVALID COLOR");
  }
}

static void runPickDropFlow() {
  setMagnet(false);

  while (true) {
    int source = promptColor("PICK-UP COLOR");
    if (source < 0) return;

    int destination = promptColor("DROP COLOR");
    if (destination < 0) return;

    if (source == destination) {
      Serial.println("Pickup and drop colors are the same; choose again.");
      continue;
    }

    assignmentPrompt = "FIND PICKUP";
    if (!liftTo(LIFT_UP_ANGLE)) {
      Serial.println("Lift servo unavailable; aborting.");
      return;
    }

    int sourceAngle = 0;
    if (!findRgbColor(source, sourceAngle)) {
      Serial.println("Pickup color not found; choose again.");
      continue;
    }

    assignmentPrompt = "LOWER / PICK";
    bool objectSensed = lowerUntilObject();
    if (!objectSensed) {
      Serial.println("Continuing to pickup at the down limit.");
    }

    setMagnet(true);
    Serial.print("Holding magnet for ");
    Serial.print(MAGNET_PICK_MS);
    Serial.println(" ms.");
    wifiServiceDelay(MAGNET_PICK_MS);

    if (!liftTo(LIFT_UP_ANGLE)) {
      setMagnet(false);
      return;
    }

    assignmentPrompt = "FIND DROP";
    int destinationAngle = 0;
    if (!findRgbColor(destination, destinationAngle)) {
      setMagnet(false);
      Serial.println("Drop color not found; aborting this cycle.");
      continue;
    }

    assignmentPrompt = "LOWER / DROP";
    if (!liftTo(LIFT_DOWN_ANGLE)) {
      setMagnet(false);
      return;
    }

    wifiServiceDelay(LIFT_SETTLE_MS);
    setMagnet(false);
    wifiServiceDelay(MAGNET_DROP_MS);
    liftTo(LIFT_UP_ANGLE);

    Serial.println("Pick-and-place cycle complete.");
    assignmentPrompt = "MENU";
    return;
  }
}

void runCalibrationFlow(bool manual, bool automatic) {
  mode = MODE_CALIBRATION;
  assignmentPrompt = "LIFT UP";
  liftTo(LIFT_UP_ANGLE);

  if (manual) {
    assignmentPrompt = "MANUAL CAL";
    Serial.println("Manual RGB calibration: place the LDR over the white patch at WHITE_PATCH_ANGLE.");
    ManualCalibrate();
  }
  else if (automatic) {
    assignmentPrompt = "AUTO CAL";
    Serial.println("Auto RGB calibration: place the LDR over the white patch at WHITE_PATCH_ANGLE.");
    AutoCalibrate();
  }
  else {
    initRgbCalibrationDefaults();
    Serial.println("Using the current or default RGB calibration.");
  }

  assignmentPrompt = "BASE SWEEP";
  Serial.println("Calibration phase done. The arm does not know color locations yet;");
  Serial.println("it will now sweep 0..120 deg and classify each angle from reflected R/G/B.");
  sweepAndDisplayRgb();

  assignmentPrompt = "CHOOSE COLORS";
  Serial.println("Base sweep done. Next: enter the pickup color, then the drop color.");
  runPickDropFlow();
  assignmentPrompt = "MENU";
  mode = MODE_MENU;
}
