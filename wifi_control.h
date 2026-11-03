#pragma once

void wifiControlBegin();

// Call every loop. Handles connection retries, the HTTP server and
// queued remote commands.
void wifiControlPoll();
