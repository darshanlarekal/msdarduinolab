#include "menu.h"

#include <Arduino.h>
#include <string.h>

#include "assignment_flow.h"
#include "command_input.h"
#include "modes.h"

Mode mode = MODE_MENU;

void printMenuPrompt() {
  Serial.println();
  Serial.println("White-patch RGB calibration:");
  Serial.println("  man  = manual: adjust the three pots");
  Serial.println("  auto = automatic LED balancing");
  Serial.println("  none = use current/default calibration");
}

void handleMenuInput() {
  if (!readCommand()) return;

  const char* token = currentCommand();

  if (strcmp(token, "man") == 0) {
    runCalibrationFlow(true, false);
    printMenuPrompt();
    return;
  }

  if (strcmp(token, "auto") == 0) {
    runCalibrationFlow(false, true);
    printMenuPrompt();
    return;
  }

  if (strcmp(token, "none") == 0) {
    runCalibrationFlow(false, false);
    printMenuPrompt();
    return;
  }

  Serial.println("Invalid input. Enter man, auto, or none.");
}
