#include "color_data.h"

#include <Arduino.h>

#include "config.h"

// Boot defaults used by the white-patch calibration.
const ColorData defaultColors[] = {
  {"WHITE", 532.54, 547.10, 490.02, 442.82},
  {"GREEN", 835.46, 834.04, 756.88, 725.46},
  {"RED",   754.56, 806.42, 784.76, 713.64},
  {"BLUE",  822.54, 821.76, 809.80, 775.26}
};

static_assert(
  sizeof(defaultColors) / sizeof(defaultColors[0]) == NUM_COLORS,
  "NUM_COLORS must match defaultColors"
);

void setRGB(int red, int green, int blue) {
  analogWrite(RED_PIN, red);
  analogWrite(GREEN_PIN, green);
  analogWrite(BLUE_PIN, blue);
}

void turnOffRGB() {
  setRGB(0, 0, 0);
}

int stripIndexFor(char strip) {
  switch (strip) {
    case 'w': return 0;  // WHITE
    case 'g': return 1;  // GREEN
    case 'r': return 2;  // RED
    case 'b': return 3;  // BLUE
  }

  return -1;
}
