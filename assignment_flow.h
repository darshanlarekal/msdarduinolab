#pragma once

// Run the white-patch calibration selected by the user, sweep the base
// servo, then prompt for pickup/drop colors and execute one cycle.
void runCalibrationFlow(bool manual, bool automatic);

// Current human-readable assignment phase, shown by the web status.
const char* assignmentPromptText();
