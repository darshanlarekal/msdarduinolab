#pragma once

// Set up state only. The servo is attached on the first movement so a
// fresh flash/reset does not drive it to a default position.
void initServo();

// Move the M2 arm to an angle and log it.
void moveServo(int angle);

// Current commanded angle, for status reporting.
int servoAngleNow();

// Hunt down a color with the LDR and park the arm on it.
// Returns true when the command was a strip command.
bool seekForCommand();

// Sweep for one calibrated color. Returns its angle, or -1 when the
// color was not found.
int seekColor(int targetIndex);

// Jogging, limits and the scan report. Returns true when handled.
bool handleServoUtilityCommand();
