#pragma once

// RGB white-patch calibration state. Levels are the PWM values used for
// the three LEDs when measuring a surface.
extern bool rgbCalibrated;
extern int rgbRedLevel;
extern int rgbGreenLevel;
extern int rgbBlueLevel;
extern float rgbWhiteRed;
extern float rgbWhiteGreen;
extern float rgbWhiteBlue;

// Load compile-time defaults without overwriting a completed calibration.
void initRgbCalibrationDefaults();

// White-patch calibration routines.
void ManualCalibrate();
void AutoCalibrate();

// Measure the current surface using the calibrated LED levels.
void readCalibratedRgb(float& red, float& green, float& blue);

// 0 WHITE, 1 GREEN, 2 RED, 3 BLUE, -1 no object.
int classifyRgb(float red, float green, float blue);

// Sweep the base servo and display RGB + detected color at each step.
void sweepAndDisplayRgb();

// Sweep the base servo and stop at the first requested color.
bool findRgbColor(int targetIndex, int& foundAngle);
