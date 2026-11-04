#pragma once

// Set up state only. The servo is attached on the first movement so a
// fresh flash/reset does not drive it to a default position.
void initServo();

// Move the M2 arm to an angle and log it.
void moveServo(int angle);

// Current commanded angle, for status reporting.
int servoAngleNow();
