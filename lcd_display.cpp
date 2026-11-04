#include "lcd_display.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "config.h"

#if !LCD_ENABLED

// LCD output is temporarily disabled; keep the call sites compiling without
// touching any LCD pins. Re-enable with LCD_ENABLED in config.h.
void lcdBegin() {}
void lcdClear() {}
void lcdSetCursor(int, int) {}
void lcdPrint(const char*) {}
void lcdShowMessage(const char*, const char*) {}
void lcdShowRgb(float, float, float, const char*) {}
void lcdShowCalibration(float, float, float) {}

#else

#if LCD_USE_I2C

#include <Wire.h>

// PCF8574 backpack mapping.
static const uint8_t LCD_BACKLIGHT = 0x08;
static const uint8_t LCD_EN = 0x04;
static const uint8_t LCD_RS = 0x01;

static void lcdI2cWrite(uint8_t value) {
  Wire.beginTransmission(LCD_I2C_ADDRESS);
  Wire.write(value | LCD_BACKLIGHT);
  Wire.endTransmission();
}

static void lcdI2cPulse(uint8_t value) {
  lcdI2cWrite(value | LCD_EN);
  delayMicroseconds(1);
  lcdI2cWrite(value & ~LCD_EN);
  delayMicroseconds(50);
}

static void lcdI2cNibble(uint8_t nibble, uint8_t rs) {
  uint8_t value = (uint8_t)((nibble << 4) | rs);
  lcdI2cWrite(value);
  lcdI2cPulse(value);
}

static void lcdI2cByte(uint8_t value, uint8_t rs) {
  lcdI2cNibble(value >> 4, rs);
  lcdI2cNibble(value & 0x0F, rs);
}

void lcdBegin() {
  Wire.begin();

  delay(50);
  lcdI2cNibble(0x03, 0);
  delay(5);
  lcdI2cNibble(0x03, 0);
  delay(1);
  lcdI2cNibble(0x03, 0);
  delay(1);
  lcdI2cNibble(0x02, 0);  // switch to 4-bit mode
  delay(1);

  lcdI2cByte(0x28, 0);  // 2 lines, 5x8 font
  lcdI2cByte(0x0C, 0);  // display on, cursor off
  lcdI2cByte(0x06, 0);  // left-to-right entry
  lcdI2cByte(0x01, 0);  // clear
  delay(2);
}

void lcdClear() {
  lcdI2cByte(0x01, 0);
  delay(2);
}

void lcdSetCursor(int column, int row) {
  uint8_t address = (uint8_t)(column & 0x3F);

  if (row > 0) address |= 0x40;

  lcdI2cByte((uint8_t)(0x80 | address), 0);
}

void lcdPrint(const char* text) {
  while (*text != '\0') {
    lcdI2cByte((uint8_t)*text++, LCD_RS);
  }
}

#else

#include <LiquidCrystal.h>

static LiquidCrystal lcd(
  LCD_RS_PIN,
  LCD_EN_PIN,
  LCD_D4_PIN,
  LCD_D5_PIN,
  LCD_D6_PIN,
  LCD_D7_PIN
);

void lcdBegin() {
  lcd.begin(LCD_COLUMNS, LCD_ROWS);
  lcd.clear();
}

void lcdClear() {
  lcd.clear();
}

void lcdSetCursor(int column, int row) {
  lcd.setCursor(column, row);
}

void lcdPrint(const char* text) {
  while (*text != '\0') {
    lcd.write((uint8_t)*text++);
  }
}

#endif

static void lcdWriteLine(int row, const char* text) {
  char line[LCD_COLUMNS + 1];

  int length = (int)strlen(text);
  if (length > LCD_COLUMNS) length = LCD_COLUMNS;

  memcpy(line, text, length);

  for (int i = length; i < LCD_COLUMNS; i++) {
    line[i] = ' ';
  }

  line[LCD_COLUMNS] = '\0';

  lcdSetCursor(0, row);
  lcdPrint(line);
}

void lcdShowMessage(const char* line0, const char* line1) {
  lcdWriteLine(0, line0);
  lcdWriteLine(1, line1);
}

void lcdShowRgb(float red, float green, float blue, const char* colorName) {
  char line[LCD_COLUMNS + 1];

  snprintf(
    line,
    sizeof(line),
    "R:%4d G:%4d",
    (int)(red + 0.5f),
    (int)(green + 0.5f)
  );
  lcdWriteLine(0, line);

  snprintf(
    line,
    sizeof(line),
    "B:%4d %s",
    (int)(blue + 0.5f),
    colorName
  );
  lcdWriteLine(1, line);
}

void lcdShowCalibration(float red, float green, float blue) {
  char line[LCD_COLUMNS + 1];

  snprintf(
    line,
    sizeof(line),
    "R:%4d G:%4d",
    (int)(red + 0.5f),
    (int)(green + 0.5f)
  );
  lcdWriteLine(0, line);

  snprintf(
    line,
    sizeof(line),
    "B:%4d 1=DONE",
    (int)(blue + 0.5f)
  );
  lcdWriteLine(1, line);
}

#endif
