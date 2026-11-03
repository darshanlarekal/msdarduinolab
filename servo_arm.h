#pragma once

void attachServo();

// Hunt down a color with the LDR and park the arm on it.
// Returns true when the command was a strip command.
bool seekForCommand();

// Jogging, limits and the scan report. Returns true when handled.
bool handleServoUtilityCommand();
