#pragma once

// Each row contains:
// strip name, red-light, blue-light, green-light, white-light
struct ColorData {
  const char* name;
  float red;
  float blue;
  float green;
  float white;
};

extern const ColorData defaultColors[];

// Values actually used by the detector. Sensing overwrites these
// in RAM; "reuse old values" copies defaultColors back in.
extern ColorData colors[];

// Copy the saved table into the live detector table.
void loadOldValues();

// LED control.
void showColor(int index);
void setRGB(int red, int green, int blue);
void turnOffRGB();

// Map the strip label typed at the prompt onto the detector row.
int stripIndexFor(char strip);

// Print the table the detector is currently comparing against.
void printDetectorTable();
