#pragma once

void initPickAndPlace();

// Manual lift, magnet and full pick/place commands.
// Returns true when the token was handled.
bool handlePickPlaceCommand();

// Move the lift to an angle. Returns false if the servo cannot attach.
bool liftTo(int angle);

// Set the electromagnet state.
void setMagnet(bool on);

// True when the IR object sensor is triggered.
bool objectDetected();

// Lower the lift from the up position until the object sensor triggers or
// the down limit is reached. Returns true if the sensor triggered.
bool lowerUntilObject();

int liftAngleNow();
bool magnetIsOn();
