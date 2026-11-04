#pragma once

#include <Arduino.h>

// -------- PINS --------
const int LDR_PIN = A2;
const int RED_PIN = 9;
const int GREEN_PIN = 10;
const int BLUE_PIN = 11;

// -------- LCD --------
// 0 = parallel 16x2 LCD, 1 = PCF8574 I2C backpack (SDA A4, SCL A5).
// The parallel pins below are defaults only; change them to match the
// actual wiring. D5 is reserved for the object sensor.
#define LCD_USE_I2C 0
const int LCD_COLUMNS = 16;
const int LCD_ROWS = 2;
const int LCD_I2C_ADDRESS = 0x27;

const int LCD_RS_PIN = 2;
const int LCD_EN_PIN = 3;
const int LCD_D4_PIN = 4;
const int LCD_D5_PIN = 7;
const int LCD_D6_PIN = 8;
const int LCD_D7_PIN = 13;

// -------- SERVO (M2) --------
// Signal wire of the servo plugged into M2. The board silkscreen marks the
// M2 header as 6 / + / -, so the signal pin is D6. Board DIP switch 8
// connects D6 to M2.
const int SERVO_PIN = 6;

// The LDR sits on the servo, so positions are not hardcoded per color:
// the arm sweeps and the calibration table decides where it is.
const int SERVO_HOME_ANGLE = 90;

// Base angle over the white reference patch used for RGB balancing.
const int WHITE_PATCH_ANGLE = 85;

// Mechanical travel of this rig, measured once: the arm goes 0..120
// and must never be driven beyond that.
const int SERVO_MIN_ANGLE = 0;
const int SERVO_MAX_ANGLE = 120;

// Sweep used while hunting for a color.
const int SERVO_STEP_DEG = 5;
const unsigned long SERVO_STEP_DELAY_MS = 80;
const int SEEK_STEP_DEG = SERVO_STEP_DEG;
const int SEEK_SETTLE_MS = 150;
const int SEEK_SAMPLES = 8;
const int SEEK_REQUIRED_HITS = 2;
const int JOG_STEP_DEG = SERVO_STEP_DEG;

// A reading further than this from every calibration row is treated as
// "nothing under the sensor" instead of the closest color.
const float SEEK_MAX_MATCH_DISTANCE = 150.0f;

// -------- LIFT / PICK (A0) --------
// Up/down servo signal on A0. Power the servo from 5V/GND, not from A0.
const int LIFT_SERVO_PIN = A0;

// IR object sensor on M1 / D5. The module used in ir_test is active low.
// Disabled until the sensor is fitted; the lowering code remains in place.
const bool OBJECT_SENSOR_ENABLED = false;
const int OBJECT_SENSOR_PIN = 5;
const bool OBJECT_SENSOR_ACTIVE_LOW = true;
const unsigned long OBJECT_SETTLE_MS = 100;

// Electromagnet driver on D12 (MOSFET/relay; do not drive the coil directly).
const int MAGNET_PIN = 12;
const bool MAGNET_ACTIVE_HIGH = true;

// Measured mechanical limits: 24 deg is fully up, 54 deg is fully down.
const int LIFT_UP_ANGLE = 24;
const int LIFT_DOWN_ANGLE = 54;
const int LIFT_MIN_ANGLE = LIFT_UP_ANGLE;
const int LIFT_MAX_ANGLE = LIFT_DOWN_ANGLE;
const int LIFT_FINE_JOG_DEG = 3;

// M2 destination after the object is picked up.
const int PLACE_ANGLE = 90;

const unsigned long LIFT_SETTLE_MS = 500;
const unsigned long PICK_MOVE_SETTLE_MS = 500;
const unsigned long MAGNET_PICK_MS = 5000;  // required pickup hold
const unsigned long MAGNET_DROP_MS = 300;

// -------- RGB WHITE-PATCH CALIBRATION --------
// Delay before each LDR read after switching a calibration LED on.
const int RGB_READ_DELAY_MS = 80;
const int RGB_CAL_LOOP_MS = 200;
const int RGB_CAL_MAX_LEVEL = 255;
const int RGB_AUTO_SEARCH_STEPS = 8;
const float RGB_EQUAL_TOLERANCE = 15.0f;
// White/color decisions use channel readings normalized to the white
// reference. Three percent is tighter than the old raw 20% window, which
// became enormous when the white reading was near 900.
const float RGB_WHITE_SPREAD_RATIO = 0.03f;
const float RGB_DARK_RATIO = 1.60f;
// Do not let a high white reading push the dark cutoff above the ADC range.
const float RGB_DARK_LEVEL = 950.0f;
const float RGB_DEFAULT_WHITE = 500.0f;
// Warn when the white patch looks very dark. This usually means the LDR is
// not over the white patch, the angle is wrong, or the pots are too high.
const float RGB_TARGET_WARN_LEVEL = 800.0f;

// -------- DETECTOR SETTINGS --------
const int SAMPLES = 20;
const int REQUIRED_STABLE_SCANS = 3;
const int SCAN_DELAY_MS = 40;

// -------- CALIBRATION SETTINGS --------
const int NUM_READINGS = 50;
const int SETTLE_TIME_MS = 200;
const int READING_DELAY_MS = 10;

// -------- COMMAND INPUT --------
// One typed command, lower case, without spaces or line endings.
const int CMD_BUF_SIZE = 12;

// Characters are collected until the line ends, so "20" stays "20"
// instead of being read as "2" then "0". A long pause also ends the
// line, for Serial Monitor setups that send no line ending.
const unsigned long CMD_IDLE_MS = 300;

// Number of rows in the detector table (WHITE, GREEN, RED, BLUE).
const int NUM_COLORS = 4;
