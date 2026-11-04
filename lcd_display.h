#pragma once

// Initialize the LCD. Parallel pins or I2C are selected in config.h.
void lcdBegin();

// Low-level display helpers.
void lcdClear();
void lcdSetCursor(int column, int row);
void lcdPrint(const char* text);

// Write a full line, padding it to the LCD width.
void lcdShowMessage(const char* line0, const char* line1);

// Show the three reflected values and the classified color.
void lcdShowRgb(float red, float green, float blue, const char* colorName);

// Show R/G/B while the user adjusts the three potentiometers.
void lcdShowCalibration(float red, float green, float blue);
