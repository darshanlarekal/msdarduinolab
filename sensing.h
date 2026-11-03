#pragma once

// Average of SAMPLES LDR readings (detector measurement).
float readAverageLDR();

// Compare a reading against the calibration column matching the LED's
// current displayed color.
int classifyReading(float reading);

// Closest strip under white illumination, or -1 when the reading is
// too far from every row to be any of them.
int classifyUnderWhite(float reading);

// Average of SEEK_SAMPLES LDR readings (servo sweep measurement).
float readSeekAverage();
