#pragma once

#include <Arduino.h>

// -------- PINS --------
const int LDR_PIN = A2;
const int RED_PIN = 9;
const int GREEN_PIN = 10;
const int BLUE_PIN = 11;

// -------- SERVO (M2) --------
// Signal wire of the servo plugged into M2. The board silkscreen marks the
// M2 header as 6 / + / -, so the signal pin is D6. Board DIP switch 8
// connects D6 to M2.
const int SERVO_PIN = 6;

// The LDR sits on the servo, so positions are not hardcoded per color:
// the arm sweeps and the calibration table decides where it is.
const int SERVO_HOME_ANGLE = 90;

// Mechanical travel of this rig, measured once: the arm goes 0..120
// and must never be driven beyond that.
const int SERVO_MIN_ANGLE = 0;
const int SERVO_MAX_ANGLE = 120;

// Sweep used while hunting for a color.
const int SEEK_STEP_DEG = 3;
const int SEEK_SETTLE_MS = 150;
const int SEEK_SAMPLES = 8;
const int SEEK_REQUIRED_HITS = 2;
const int JOG_STEP_DEG = 4;

// A reading further than this from every calibration row is treated as
// "nothing under the sensor" instead of the closest color.
const float SEEK_MAX_MATCH_DISTANCE = 150.0f;

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
