#pragma once

void attachPickAndPlace();

// Manual lift, magnet and full pick/place commands.
// Returns true when the token was handled.
bool handlePickPlaceCommand();

int liftAngleNow();
bool magnetIsOn();
