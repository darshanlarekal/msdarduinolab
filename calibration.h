#pragma once

// Store the four readings for each strip.
struct ColorCalibration {
  char strip;
  float redLight;
  float blueLight;
  float greenLight;
  float whiteLight;
};

extern int stripsCalibrated;

void printPrompt();

void printAllResults();

// Measure one strip under red, blue, green and white light, then feed
// the result into the detector's table.
void calibrateStrip(char strip);
