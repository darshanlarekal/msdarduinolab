#include "sensing.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

#include "color_data.h"
#include "config.h"
#include "detection.h"

float readAverageLDR() {
  long total = 0;

  for (int i = 0; i < SAMPLES; i++) {
    total += analogRead(LDR_PIN);
    delay(2);
  }

  return (float)total / SAMPLES;
}

// Get the expected calibration reading under
// the color currently displayed by the LED.
static float expectedReading(int stripIndex, int lightIndex) {
  switch (lightIndex) {
    case 0: return colors[stripIndex].white;
    case 1: return colors[stripIndex].red;
    case 2: return colors[stripIndex].green;
    case 3: return colors[stripIndex].blue;
  }

  return 0;
}

static int currentLightIndex() {
  const char* name = colors[displayedColor].name;

  if (strcmp(name, "RED") == 0) return 1;
  if (strcmp(name, "GREEN") == 0) return 2;
  if (strcmp(name, "BLUE") == 0) return 3;

  return 0;  // WHITE
}

int classifyReading(float reading) {
  int lightIndex = currentLightIndex();

  int bestIndex = 0;
  float bestDifference = INFINITY;

  for (int i = 0; i < NUM_COLORS; i++) {
    float expected = expectedReading(i, lightIndex);
    float difference = fabs(reading - expected);

    if (difference < bestDifference) {
      bestDifference = difference;
      bestIndex = i;
    }
  }

  return bestIndex;
}

int classifyUnderWhite(float reading) {
  int bestIndex = -1;
  float bestDifference = INFINITY;

  for (int i = 0; i < NUM_COLORS; i++) {
    float difference = fabs(reading - colors[i].white);

    if (difference < bestDifference) {
      bestDifference = difference;
      bestIndex = i;
    }
  }

  if (bestDifference > SEEK_MAX_MATCH_DISTANCE) return -1;

  return bestIndex;
}

float readSeekAverage() {
  long total = 0;

  for (int i = 0; i < SEEK_SAMPLES; i++) {
    total += analogRead(LDR_PIN);
    delay(2);
  }

  return (float)total / SEEK_SAMPLES;
}
