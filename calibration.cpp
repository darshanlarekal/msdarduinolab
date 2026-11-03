#include "calibration.h"

#include <Arduino.h>

#include "color_data.h"
#include "config.h"

static ColorCalibration results[4];
int stripsCalibrated = 0;

static float getAverageLDR() {
  long total = 0;

  for (int i = 0; i < NUM_READINGS; i++) {
    total += analogRead(LDR_PIN);
    delay(READING_DELAY_MS);
  }

  return (float)total / NUM_READINGS;
}

static float measureUnderLight(
  const char* lightName,
  int red,
  int green,
  int blue
) {
  Serial.print("Shining ");
  Serial.println(lightName);

  setRGB(red, green, blue);
  delay(SETTLE_TIME_MS);

  float average = getAverageLDR();

  Serial.print("  Average LDR = ");
  Serial.println(average, 2);

  turnOffRGB();
  delay(100);

  return average;
}

// Feed a fresh calibration row into the detector's table.
static void applyCalibrationToDetector(const ColorCalibration& data) {
  int index = stripIndexFor(data.strip);

  if (index < 0) return;

  colors[index].red = data.redLight;
  colors[index].blue = data.blueLight;
  colors[index].green = data.greenLight;
  colors[index].white = data.whiteLight;
}

void calibrateStrip(char strip) {
  ColorCalibration data;
  data.strip = strip;

  Serial.println();
  Serial.println("================================");
  Serial.print("CALIBRATING STRIP: ");
  Serial.println(strip);
  Serial.println("Keep the sensor stationary.");
  Serial.println("================================");

  // Red illumination
  data.redLight = measureUnderLight(
    "RED", 255, 0, 0
  );

  // Blue illumination
  data.blueLight = measureUnderLight(
    "BLUE", 0, 0, 255
  );

  // Green illumination
  data.greenLight = measureUnderLight(
    "GREEN", 0, 255, 0
  );

  // White illumination (all RGB channels on)
  data.whiteLight = measureUnderLight(
    "WHITE", 255, 255, 255
  );

  // Save calibration results
  results[stripsCalibrated] = data;
  stripsCalibrated++;

  applyCalibrationToDetector(data);

  Serial.println();
  Serial.println("------ STRIP RESULTS ------");

  Serial.print("Strip label: ");
  Serial.println(data.strip);

  Serial.print("Red light average:   ");
  Serial.println(data.redLight, 2);

  Serial.print("Blue light average:  ");
  Serial.println(data.blueLight, 2);

  Serial.print("Green light average: ");
  Serial.println(data.greenLight, 2);

  Serial.print("White light average: ");
  Serial.println(data.whiteLight, 2);

  Serial.println("---------------------------");
  Serial.println("Results saved in this run.");
}

void printPrompt() {
  Serial.println();
  Serial.println("Place the LDR over a strip:");
  Serial.println("  < / > = jog left/right, or type an angle to jump");
  Serial.println("  k = show limits, v = scan what the LDR sees");
  Serial.println("  u/l = lift up/down, m = magnet, p = pick/place");
  Serial.println("  + / - = fine lift 3 deg up/down");
  Serial.println("Enter strip color: w, b, g, or r");
  Serial.println("Type x to print all results.");
  Serial.println("Type t to start detection.");
  Serial.println("Enter the next label when ready.");
}

void printAllResults() {
  Serial.println();
  Serial.println("========== ALL CALIBRATION DATA ==========");
  Serial.println("Label,RedLight,BlueLight,GreenLight,WhiteLight");

  for (int i = 0; i < stripsCalibrated; i++) {
    Serial.print(results[i].strip);
    Serial.print(",");
    Serial.print(results[i].redLight, 2);
    Serial.print(",");
    Serial.print(results[i].blueLight, 2);
    Serial.print(",");
    Serial.print(results[i].greenLight, 2);
    Serial.print(",");
    Serial.println(results[i].whiteLight, 2);
  }

  Serial.println("==========================================");
}
