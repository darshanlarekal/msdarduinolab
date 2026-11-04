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

// LED control.
void setRGB(int red, int green, int blue);
void turnOffRGB();

// Map the strip label typed at the prompt onto the color table.
int stripIndexFor(char strip);
