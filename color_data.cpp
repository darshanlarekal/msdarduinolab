#include "color_data.h"

#include <Arduino.h>
#include <string.h>

#include "config.h"

// Boot defaults. These live in flash and are loaded by setup().
const ColorData defaultColors[] = {
  {"WHITE", 514.04, 513.72, 468.86, 424.24},
  {"GREEN", 838.00, 831.50, 765.96, 737.00},
  {"RED",   702.64, 756.08, 726.56, 653.74},
  {"BLUE",  785.72, 781.34, 758.02, 725.12}
};

// Earlier calibration set, kept for reference:
//   {"WHITE", 649.08, 674.22, 581.26, 489.02},
//   {"GREEN", 854.10, 851.18, 757.96, 706.48},
//   {"RED",   732.98, 785.88, 736.24, 645.36},
//   {"BLUE",  891.90, 886.88, 836.34, 774.64}

static_assert(
  sizeof(defaultColors) / sizeof(defaultColors[0]) == NUM_COLORS,
  "NUM_COLORS must match defaultColors"
);

ColorData colors[NUM_COLORS];

void loadOldValues() {
  for (int i = 0; i < NUM_COLORS; i++) {
    colors[i] = defaultColors[i];
  }
}

void showColor(int index) {
  const char* name = colors[index].name;

  if (strcmp(name, "RED") == 0) {
    analogWrite(RED_PIN, 255);
    analogWrite(GREEN_PIN, 0);
    analogWrite(BLUE_PIN, 0);
  }
  else if (strcmp(name, "GREEN") == 0) {
    analogWrite(RED_PIN, 0);
    analogWrite(GREEN_PIN, 255);
    analogWrite(BLUE_PIN, 0);
  }
  else if (strcmp(name, "BLUE") == 0) {
    analogWrite(RED_PIN, 0);
    analogWrite(GREEN_PIN, 0);
    analogWrite(BLUE_PIN, 255);
  }
  else {
    // WHITE: all three channels on
    analogWrite(RED_PIN, 255);
    analogWrite(GREEN_PIN, 255);
    analogWrite(BLUE_PIN, 255);
  }
}

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

void printDetectorTable() {
  Serial.println();
  Serial.println("========== VALUES IN USE ==========");
  Serial.println("Strip,RedLight,BlueLight,GreenLight,WhiteLight");

  for (int i = 0; i < NUM_COLORS; i++) {
    Serial.print(colors[i].name);
    Serial.print(",");
    Serial.print(colors[i].red, 2);
    Serial.print(",");
    Serial.print(colors[i].blue, 2);
    Serial.print(",");
    Serial.print(colors[i].green, 2);
    Serial.print(",");
    Serial.println(colors[i].white, 2);
  }

  Serial.println("==================================");
}
