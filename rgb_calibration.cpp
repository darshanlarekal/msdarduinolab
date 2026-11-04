#include "rgb_calibration.h"

#include <Arduino.h>
#include <math.h>
#include <stdio.h>

#include "color_data.h"
#include "command_input.h"
#include "config.h"
#include "lcd_display.h"
#include "sensing.h"
#include "servo_arm.h"
#include "wifi_control.h"

bool rgbCalibrated = false;
int rgbRedLevel = RGB_CAL_MAX_LEVEL;
int rgbGreenLevel = RGB_CAL_MAX_LEVEL;
int rgbBlueLevel = RGB_CAL_MAX_LEVEL;
float rgbWhiteRed = RGB_DEFAULT_WHITE;
float rgbWhiteGreen = RGB_DEFAULT_WHITE;
float rgbWhiteBlue = RGB_DEFAULT_WHITE;

static float maximumOf(float a, float b, float c) {
  float result = a;
  if (b > result) result = b;
  if (c > result) result = c;
  return result;
}

static float minimumOf(float a, float b, float c) {
  float result = a;
  if (b < result) result = b;
  if (c < result) result = c;
  return result;
}

// Read one LDR channel at the requested PWM level. The 80 ms delay is the
// "small delay" required before each calibration reading.
static float readChannelLevel(int channel, int level) {
  if (level < 0) level = 0;
  if (level > RGB_CAL_MAX_LEVEL) level = RGB_CAL_MAX_LEVEL;

  if (channel == 0) {
    setRGB(level, 0, 0);
  }
  else if (channel == 1) {
    setRGB(0, level, 0);
  }
  else {
    setRGB(0, 0, level);
  }

  wifiServiceDelay(RGB_READ_DELAY_MS);

  float value = readAverageLDR();

  turnOffRGB();
  wifiServiceDelay(20);

  return value;
}

static void readRgbAtLevels(
  int redLevel,
  int greenLevel,
  int blueLevel,
  float& red,
  float& green,
  float& blue
) {
  red = readChannelLevel(0, redLevel);
  green = readChannelLevel(1, greenLevel);
  blue = readChannelLevel(2, blueLevel);

  Serial.print("RGB levels ");
  Serial.print(redLevel);
  Serial.print("/");
  Serial.print(greenLevel);
  Serial.print("/");
  Serial.print(blueLevel);
  Serial.print(" -> R ");
  Serial.print(red, 1);
  Serial.print(" G ");
  Serial.print(green, 1);
  Serial.print(" B ");
  Serial.println(blue, 1);
}

static void positionOverWhitePatch() {
  Serial.print("Moving base to white patch at ");
  Serial.print(WHITE_PATCH_ANGLE);
  Serial.println(" deg.");
  moveServo(WHITE_PATCH_ANGLE);
  wifiServiceDelay(LIFT_SETTLE_MS);
}

// Find the PWM level whose LDR reading is closest to the target. The LDR
// reading falls as the LED gets brighter, so this is a descending search.
static int findLevelForTarget(int channel, float target) {
  int low = 0;
  int high = RGB_CAL_MAX_LEVEL;
  int bestLevel = RGB_CAL_MAX_LEVEL;
  float bestDifference = 1000000.0f;

  for (int i = 0; i < RGB_AUTO_SEARCH_STEPS; i++) {
    int mid = (low + high) / 2;
    float value = readChannelLevel(channel, mid);
    float difference = fabs(value - target);

    if (difference < bestDifference) {
      bestDifference = difference;
      bestLevel = mid;
    }

    if (value > target) {
      // Too dim: increase the LED level.
      low = mid + 1;
    }
    else {
      // Too bright: decrease the LED level.
      high = mid - 1;
    }
  }

  return bestLevel;
}

void initRgbCalibrationDefaults() {
  if (!rgbCalibrated) {
    rgbRedLevel = RGB_CAL_MAX_LEVEL;
    rgbGreenLevel = RGB_CAL_MAX_LEVEL;
    rgbBlueLevel = RGB_CAL_MAX_LEVEL;
    rgbWhiteRed = defaultColors[0].red;
    rgbWhiteGreen = defaultColors[0].green;
    rgbWhiteBlue = defaultColors[0].blue;
  }
}

void ManualCalibrate() {
  positionOverWhitePatch();

  lcdShowMessage("MANUAL CAL", "ADJUST POTS");
  Serial.println();
  Serial.println("MANUAL WHITE-PATCH CALIBRATION");
  Serial.println("1. Keep all three pots at minimum resistance.");
  Serial.println("2. Adjust the two brighter channels until R/G/B match.");
  Serial.println("3. Type 1 and press Enter when done.");

  float red = 0;
  float green = 0;
  float blue = 0;

  while (true) {
    readRgbAtLevels(
      RGB_CAL_MAX_LEVEL,
      RGB_CAL_MAX_LEVEL,
      RGB_CAL_MAX_LEVEL,
      red,
      green,
      blue
    );

    lcdShowCalibration(red, green, blue);

    float maximum = maximumOf(red, green, blue);

    Serial.print("R:");
    Serial.print(red, 1);
    Serial.print(" G:");
    Serial.print(green, 1);
    Serial.print(" B:");
    Serial.print(blue, 1);
    Serial.print("  MAX=");
    Serial.println(maximum, 1);

    if (readCommand()) {
      if (isCommand("1", '1')) {
        // Capture one final reading after the user's last pot adjustment.
        readRgbAtLevels(
          RGB_CAL_MAX_LEVEL,
          RGB_CAL_MAX_LEVEL,
          RGB_CAL_MAX_LEVEL,
          red,
          green,
          blue
        );
        break;
      }

      Serial.println("Manual calibration is active. Type 1 to finish.");
    }

    wifiServiceDelay(RGB_CAL_LOOP_MS);
  }

  rgbRedLevel = RGB_CAL_MAX_LEVEL;
  rgbGreenLevel = RGB_CAL_MAX_LEVEL;
  rgbBlueLevel = RGB_CAL_MAX_LEVEL;
  rgbWhiteRed = red;
  rgbWhiteGreen = green;
  rgbWhiteBlue = blue;
  rgbCalibrated = true;

  lcdShowMessage("MANUAL CAL", "SAVED");
  Serial.println("Manual white-patch calibration saved.");
}

void AutoCalibrate() {
  positionOverWhitePatch();

  lcdShowMessage("AUTO CAL", "MEASURE FULL");
  Serial.println();
  Serial.println("AUTO WHITE-PATCH CALIBRATION");
  Serial.println("Keep all three pots at minimum resistance.");

  float fullRed = 0;
  float fullGreen = 0;
  float fullBlue = 0;

  readRgbAtLevels(
    RGB_CAL_MAX_LEVEL,
    RGB_CAL_MAX_LEVEL,
    RGB_CAL_MAX_LEVEL,
    fullRed,
    fullGreen,
    fullBlue
  );

  float target = maximumOf(fullRed, fullGreen, fullBlue);

  Serial.print("Target (dimmest channel) = ");
  Serial.println(target, 1);

  if (target > RGB_TARGET_WARN_LEVEL) {
    Serial.println("WARNING: the white-patch reading is very dark.");
    Serial.println("Check WHITE_PATCH_ANGLE, that the LDR is over the white patch,");
    Serial.println("and that all three pots are at minimum resistance.");
  }

  Serial.println("Dimming the brighter channels...");

  int levels[3] = {
    RGB_CAL_MAX_LEVEL,
    RGB_CAL_MAX_LEVEL,
    RGB_CAL_MAX_LEVEL
  };
  float fullReadings[3] = {fullRed, fullGreen, fullBlue};

  for (int channel = 0; channel < 3; channel++) {
    if (fullReadings[channel] < target - RGB_EQUAL_TOLERANCE) {
      lcdShowMessage("AUTO CAL", "ADJUST LEDS");
      levels[channel] = findLevelForTarget(channel, target);

      Serial.print("Channel ");
      Serial.print(channel);
      Serial.print(" level -> ");
      Serial.println(levels[channel]);
    }
  }

  rgbRedLevel = levels[0];
  rgbGreenLevel = levels[1];
  rgbBlueLevel = levels[2];

  float finalRed = 0;
  float finalGreen = 0;
  float finalBlue = 0;

  readRgbAtLevels(
    rgbRedLevel,
    rgbGreenLevel,
    rgbBlueLevel,
    finalRed,
    finalGreen,
    finalBlue
  );

  rgbWhiteRed = finalRed;
  rgbWhiteGreen = finalGreen;
  rgbWhiteBlue = finalBlue;
  rgbCalibrated = true;

  char line[LCD_COLUMNS + 1];
  snprintf(line, sizeof(line), "%d/%d/%d", rgbRedLevel, rgbGreenLevel, rgbBlueLevel);
  lcdShowMessage("AUTO CAL DONE", line);

  Serial.print("Auto calibration levels R/G/B = ");
  Serial.print(rgbRedLevel);
  Serial.print("/");
  Serial.print(rgbGreenLevel);
  Serial.print("/");
  Serial.println(rgbBlueLevel);
  Serial.print("White reference R/G/B = ");
  Serial.print(rgbWhiteRed, 1);
  Serial.print("/");
  Serial.print(rgbWhiteGreen, 1);
  Serial.print("/");
  Serial.println(rgbWhiteBlue, 1);

  float whiteAverage =
    (rgbWhiteRed + rgbWhiteGreen + rgbWhiteBlue) / 3.0f;
  Serial.print("White average = ");
  Serial.print(whiteAverage, 1);
  Serial.print("; white spread limit = ");
  Serial.print(whiteAverage * RGB_WHITE_SPREAD_RATIO, 1);
  Serial.print("; dark threshold = ");
  Serial.println(whiteAverage * RGB_DARK_RATIO, 1);
}

void readCalibratedRgb(float& red, float& green, float& blue) {
  readRgbAtLevels(
    rgbRedLevel,
    rgbGreenLevel,
    rgbBlueLevel,
    red,
    green,
    blue
  );
}

int classifyRgb(float red, float green, float blue) {
  float whiteAverage =
    (rgbWhiteRed + rgbWhiteGreen + rgbWhiteBlue) / 3.0f;

  if (whiteAverage < 1.0f) {
    whiteAverage = RGB_DEFAULT_WHITE;
  }

  float minimum = minimumOf(red, green, blue);

  // A very high reading means very little reflected light: no object.
  // Clamp the threshold so a high white reading cannot put "none" above
  // the ADC's 1023 limit.
  float darkThreshold = whiteAverage * RGB_DARK_RATIO;
  if (darkThreshold > RGB_DARK_LEVEL) {
    darkThreshold = RGB_DARK_LEVEL;
  }

  if (minimum >= darkThreshold) {
    return -1;
  }

  // Normalize each channel to its white-patch reading. This keeps the
  // white/color decision meaningful even when the three white readings
  // are not exactly equal.
  float whiteRed = rgbWhiteRed > 1.0f ? rgbWhiteRed : 1.0f;
  float whiteGreen = rgbWhiteGreen > 1.0f ? rgbWhiteGreen : 1.0f;
  float whiteBlue = rgbWhiteBlue > 1.0f ? rgbWhiteBlue : 1.0f;

  float redRatio = red / whiteRed;
  float greenRatio = green / whiteGreen;
  float blueRatio = blue / whiteBlue;

  float maximumRatio = maximumOf(redRatio, greenRatio, blueRatio);
  float minimumRatio = minimumOf(redRatio, greenRatio, blueRatio);

  // White reflects all three channels almost equally.
  if ((maximumRatio - minimumRatio) <= RGB_WHITE_SPREAD_RATIO) {
    return 0;
  }

  // The brightest reflection has the lowest normalized LDR reading.
  if (minimumRatio == redRatio) return 2;
  if (minimumRatio == greenRatio) return 1;
  return 3;
}

void sweepAndDisplayRgb() {
  Serial.println();
  Serial.println("BASE SWEEP: angle, R, G, B, color");

  for (int angle = SERVO_MIN_ANGLE;
       angle <= SERVO_MAX_ANGLE;
       angle += SERVO_STEP_DEG) {

    moveServo(angle);

    float red = 0;
    float green = 0;
    float blue = 0;
    readCalibratedRgb(red, green, blue);

    int detected = classifyRgb(red, green, blue);
    const char* name = detected >= 0 ? colors[detected].name : "NONE";

    lcdShowRgb(red, green, blue, name);

    Serial.print(angle);
    Serial.print(",");
    Serial.print(red, 1);
    Serial.print(",");
    Serial.print(green, 1);
    Serial.print(",");
    Serial.print(blue, 1);
    Serial.print(",");
    Serial.println(name);
  }

  Serial.println("Base sweep done.");
}

bool findRgbColor(int targetIndex, int& foundAngle) {
  Serial.print("Searching pickup/drop color ");
  Serial.print(colors[targetIndex].name);
  Serial.println("...");

  for (int angle = SERVO_MIN_ANGLE;
       angle <= SERVO_MAX_ANGLE;
       angle += SERVO_STEP_DEG) {

    moveServo(angle);

    float red = 0;
    float green = 0;
    float blue = 0;
    readCalibratedRgb(red, green, blue);

    int detected = classifyRgb(red, green, blue);
    const char* name = detected >= 0 ? colors[detected].name : "NONE";

    lcdShowRgb(red, green, blue, name);

    if (detected == targetIndex) {
      foundAngle = angle;
      Serial.print(colors[targetIndex].name);
      Serial.print(" found at ");
      Serial.print(angle);
      Serial.println(" deg.");
      return true;
    }
  }

  Serial.print("No ");
  Serial.print(colors[targetIndex].name);
  Serial.println(" found.");
  return false;
}
