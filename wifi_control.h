#pragma once

void wifiControlBegin();

// Call every loop. Handles connection retries, the HTTP server and
// queued remote commands.
void wifiControlPoll();

// Delay while still serving HTTP status, so slow servo moves are
// visible on the web page.
void wifiServiceDelay(unsigned long ms);
