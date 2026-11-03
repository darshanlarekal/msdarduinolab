#include "color_data.h"

#include <Arduino.h>
#include <string.h>

#include "config.h"

// Boot defaults. These live in flash and are loaded by setup().
const ColorData defaultColors[] = {
  {"WHITE", 532.54, 547.10, 490.02, 442.82},
  {"GREEN", 835.46, 834.04, 756.88, 725.46},
  {"RED",   754.56, 806.42, 784.76, 713.64},
  {"BLUE",  822.54, 821.76, 809.80, 775.26}
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
