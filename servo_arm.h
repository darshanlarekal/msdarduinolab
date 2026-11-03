#pragma once

void attachServo();

// Move the M2 arm to an angle and log it.
void moveServo(int angle);

// Current commanded angle, for status reporting.
int servoAngleNow();

// Hunt down a color with the LDR and park the arm on it.
// Returns true when the command was a strip command.
bool seekForCommand();

// Jogging, limits and the scan report. Returns true when handled.
bool handleServoUtilityCommand();
