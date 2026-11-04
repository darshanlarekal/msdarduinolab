// Single-file version of the arduino_maintain sketch.
// All modules are merged here so the sketch can be opened and flashed as one
// .ino file. Behavior matches ~/Arduino/arduino_maintain.

#include <Arduino.h>
#include <Servo.h>
#include <WiFiS3.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// -------- WIFI CREDENTIALS --------
// Fill these in with a 2.4 GHz network. Leave both blank for serial-only
// control. These used to live in arduino_secrets.h.
#define SECRET_SSID ""
#define SECRET_PASS ""


// ===== config.h =====

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
const unsigned long MAGNET_PICK_MS = 300;
const unsigned long MAGNET_DROP_MS = 300;

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

// ===== shared types =====
struct ColorData {
  const char* name;
  float red;
  float blue;
  float green;
  float white;
};

struct ColorCalibration {
  char strip;
  float redLight;
  float blueLight;
  float greenLight;
  float whiteLight;
};

enum Mode { MODE_MENU, MODE_CALIBRATION, MODE_DETECTION };

// ===== shared globals =====
extern ColorData colors[];
extern int stripsCalibrated;
extern Mode mode;
extern int displayedColor;
extern int candidateColor;
extern int stableCount;
extern float lastLdrReading;


// ===== color_data.cpp =====

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

// ===== sensing.cpp =====

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

// ===== calibration.cpp =====

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

// ===== command_input.cpp =====

static char commandBuffer[CMD_BUF_SIZE];
static int commandLength = 0;
static unsigned long lastCharMs = 0;

static char pendingCommand[CMD_BUF_SIZE];
static bool hasPendingCommand = false;

bool submitCommand(const char* token) {
  if (hasPendingCommand || token == NULL) return false;

  size_t length = strlen(token);

  if (length == 0 || length >= CMD_BUF_SIZE) return false;

  for (size_t i = 0; i < length; i++) {
    if (isspace((unsigned char)token[i])) return false;

    pendingCommand[i] = (char)tolower((unsigned char)token[i]);
  }

  pendingCommand[length] = '\0';
  hasPendingCommand = true;

  return true;
}

static bool finishCommand() {
  commandBuffer[commandLength] = '\0';

  bool ready = commandLength > 0;

  commandLength = 0;

  return ready;
}

bool readCommand() {
  // Do not overwrite a partially typed serial command. Network commands
  // wait until the serial buffer is empty.
  if (hasPendingCommand && commandLength == 0) {
    strcpy(commandBuffer, pendingCommand);
    hasPendingCommand = false;
    return true;
  }

  while (Serial.available() > 0) {
    int character = Serial.read();

    lastCharMs = millis();

    if (character == '\n' || character == '\r') {
      if (commandLength == 0) continue;

      return finishCommand();
    }

    if (character == ' ') continue;

    if (commandLength < CMD_BUF_SIZE - 1) {
      commandBuffer[commandLength++] = (char)tolower(character);
    }
  }

  if (commandLength > 0 && (millis() - lastCharMs) > CMD_IDLE_MS) {
    return finishCommand();
  }

  return false;
}

// True when the token is either the full word or its single letter.
bool isCommand(const char* word, char letter) {
  if (strcmp(commandBuffer, word) == 0) return true;

  return commandBuffer[0] == letter && commandBuffer[1] == '\0';
}

// True when the token is only digits.
bool isNumberToken() {
  if (commandBuffer[0] == '\0') return false;

  for (int i = 0; commandBuffer[i] != '\0'; i++) {
    if (commandBuffer[i] < '0' || commandBuffer[i] > '9') return false;
  }

  return true;
}

const char* currentCommand() {
  return commandBuffer;
}

int commandNumber() {
  return atoi(commandBuffer);
}

// ===== servo_arm.cpp =====

static Servo m2Servo;
static int servoAngle = SERVO_HOME_ANGLE;
static bool servoAttached = false;

void initServo() {
  // Do not call attach() here. Attaching pulses the servo and drives it
  // to the library's default position on every reset.
  servoAngle = SERVO_HOME_ANGLE;
  servoAttached = false;
}

int servoAngleNow() {
  return servoAngle;
}

// Attach on first movement, then hold the current angle.
static bool ensureServoAttached() {
  if (servoAttached) return true;

  if (m2Servo.attach(SERVO_PIN) == 0) {
    Serial.println("M2 servo attach failed on D6.");
    return false;
  }

  // attach() leaves the signal line high until the first timer callback.
  // Drop it now so the servo does not see a long startup pulse, then send
  // the angle we already believe we are at.
  digitalWrite(SERVO_PIN, LOW);
  m2Servo.write(servoAngle);
  servoAttached = true;
  return true;
}

// Moves the arm without logging, used inside the sweep.
static bool writeServo(int angle) {
  if (!ensureServoAttached()) return false;

  servoAngle = angle;
  m2Servo.write(servoAngle);
  return true;
}

void moveServo(int angle) {
  if (!ensureServoAttached()) return;

  if (angle < SERVO_MIN_ANGLE) angle = SERVO_MIN_ANGLE;
  if (angle > SERVO_MAX_ANGLE) angle = SERVO_MAX_ANGLE;

  // Keep every commanded position on the 5-degree grid.
  angle = ((angle + SERVO_STEP_DEG / 2) / SERVO_STEP_DEG) * SERVO_STEP_DEG;

  if (angle < SERVO_MIN_ANGLE) angle = SERVO_MIN_ANGLE;
  if (angle > SERVO_MAX_ANGLE) angle = SERVO_MAX_ANGLE;

  while (servoAngle != angle) {
    int next;

    if (servoAngle < angle) {
      next = servoAngle + SERVO_STEP_DEG;
      if (next > angle) next = angle;
    } else {
      next = servoAngle - SERVO_STEP_DEG;
      if (next < angle) next = angle;
    }

    if (!writeServo(next)) return;
    wifiServiceDelay(SERVO_STEP_DELAY_MS);
  }

  Serial.print("Servo -> ");
  Serial.print(servoAngle);
  Serial.println(" deg");
}

static void reportLimits() {
  Serial.print("Travel: ");
  Serial.print(SERVO_MIN_ANGLE);
  Serial.print(" .. ");
  Serial.print(SERVO_MAX_ANGLE);
  Serial.print(" deg, arm now at ");
  Serial.print(servoAngle);
  Serial.println(" deg");
}

// Park the arm, then report where it is and what the LDR sees there.
// White light is held so the reading is comparable to the white column.
static void reportPosition() {
  setRGB(255, 255, 255);
  wifiServiceDelay(SEEK_SETTLE_MS);

  float reading = readSeekAverage();
  int detected = classifyUnderWhite(reading);

  Serial.print("Arm ");
  Serial.print(servoAngle);
  Serial.print(" deg, LDR ");
  Serial.print(reading, 1);
  Serial.print(", sees ");

  if (detected < 0) {
    Serial.println("nothing");
  } else {
    Serial.println(colors[detected].name);
  }

  reportLimits();
}

static void jogServo(int delta) {
  moveServo(servoAngle + delta);
  reportPosition();
}

// Walks the travel and prints what the LDR sees at every step,
// so you can tell whether a strip is even under the arm.
static void scanReport() {
  Serial.println();
  Serial.println("Angle,LDR,Classified");
  Serial.print("range ");
  Serial.print(SERVO_MIN_ANGLE);
  Serial.print("..");
  Serial.println(SERVO_MAX_ANGLE);

  setRGB(255, 255, 255);
  wifiServiceDelay(SEEK_SETTLE_MS);

  for (int angle = SERVO_MIN_ANGLE; angle <= SERVO_MAX_ANGLE; angle += SEEK_STEP_DEG) {
    if (!writeServo(angle)) return;
    wifiServiceDelay(SEEK_SETTLE_MS);

    float reading = readSeekAverage();
    int detected = classifyUnderWhite(reading);

    Serial.print(angle);
    Serial.print(",");
    Serial.print(reading, 1);
    Serial.print(",");

    if (detected < 0) {
      Serial.println("none");
    } else {
      Serial.println(colors[detected].name);
    }
  }

  Serial.println("Scan done.");
}

// Sweeps the arm and returns the middle of the first run of angles
// where the LDR sees the requested strip, or -1 when it never does.
int seekColor(int targetIndex) {
  int firstMatch = -1;
  int lastMatch = -1;
  int consecutiveHits = 0;

  // A sweep starts at SERVO_MIN_ANGLE. Walk there slowly instead of
  // jumping when a previous seek left the arm at the far end.
  if (servoAngle != SERVO_MIN_ANGLE) {
    moveServo(SERVO_MIN_ANGLE);
    wifiServiceDelay(SEEK_SETTLE_MS);
  }

  for (int angle = SERVO_MIN_ANGLE;
       angle <= SERVO_MAX_ANGLE;
       angle += SEEK_STEP_DEG) {

    if (!writeServo(angle)) return -1;
    wifiServiceDelay(SEEK_SETTLE_MS);

    float reading = readSeekAverage();
    int detected = classifyUnderWhite(reading);

    if (detected == targetIndex) {
      consecutiveHits++;

      if (consecutiveHits >= SEEK_REQUIRED_HITS) {
        if (firstMatch < 0) {
          firstMatch = angle - (SEEK_REQUIRED_HITS - 1) * SEEK_STEP_DEG;
        }
        lastMatch = angle;
      }
    }
    else {
      // The run is over, keep the first region found.
      if (firstMatch >= 0) break;
      consecutiveHits = 0;
    }
  }

  if (firstMatch < 0) return -1;

  return (firstMatch + lastMatch) / 2;
}

// Jogging, limits and the scan report. Returns true when handled.
bool handleServoUtilityCommand() {
  // A plain number jumps the arm straight to that angle.
  if (isNumberToken()) {
    int angle = commandNumber();

    if (angle < SERVO_MIN_ANGLE) angle = SERVO_MIN_ANGLE;
    if (angle > SERVO_MAX_ANGLE) angle = SERVO_MAX_ANGLE;

    moveServo(angle);
    reportPosition();
    return true;
  }

  if (isCommand("left", 'a') || isCommand("<", '<')) {
    jogServo(-JOG_STEP_DEG);
    return true;
  }

  if (isCommand("right", 'd') || isCommand(">", '>')) {
    jogServo(JOG_STEP_DEG);
    return true;
  }

  if (isCommand("limits", 'k')) {
    reportLimits();
    return true;
  }

  if (isCommand("scan", 'v')) {
    scanReport();
    return true;
  }

  return false;
}

// True when the token asks for a strip.
static bool isStripCommand(int& targetIndex) {
  if (isCommand("green", 'g')) {
    targetIndex = 1;
    return true;
  }

  if (isCommand("red", 'r')) {
    targetIndex = 2;
    return true;
  }

  if (isCommand("blue", 'b')) {
    targetIndex = 3;
    return true;
  }

  if (isCommand("white", 'w') || isCommand("home", 'h')) {
    targetIndex = 0;
    return true;
  }

  return false;
}

// Hunt down a color with the LDR and park the arm on it.
// Returns true when the command was a strip command.
bool seekForCommand() {
  int targetIndex = -1;

  if (!isStripCommand(targetIndex)) return false;

  // Hold white light while hunting so the comparison column is the
  // same one the sweep reads against.
  setRGB(255, 255, 255);
  wifiServiceDelay(SEEK_SETTLE_MS);

  Serial.print("Seeking ");
  Serial.print(colors[targetIndex].name);
  Serial.println(" with the LDR...");

  int angle = seekColor(targetIndex);

  if (angle < 0) {
    Serial.print("No ");
    Serial.print(colors[targetIndex].name);
    Serial.println(" under the sensor. Arm left where it was.");
    return true;
  }

  moveServo(angle);

  Serial.print(colors[targetIndex].name);
  Serial.print(" found at ");
  Serial.print(angle);
  Serial.println(" deg");

  // Detection restarts from white, matching the LED state now.
  displayedColor = 0;
  candidateColor = -1;
  stableCount = 0;

  return true;
}

// ===== pick_place.cpp =====

static Servo liftServo;
static int liftAngle = LIFT_UP_ANGLE;
static bool magnetOn = false;
static bool liftAttached = false;

static void setMagnet(bool on) {
  magnetOn = on;
  digitalWrite(MAGNET_PIN, on == MAGNET_ACTIVE_HIGH ? HIGH : LOW);

  Serial.print("Magnet ");
  Serial.println(on ? "ON" : "OFF");
}

// Attach the lift only when it first needs to move. Attaching at boot
// makes the servo jump to the library default on every reset.
static bool ensureLiftAttached() {
  if (liftAttached) return true;

  if (liftServo.attach(LIFT_SERVO_PIN) == 0) {
    Serial.println("Lift servo attach failed on A0.");
    return false;
  }

  digitalWrite(LIFT_SERVO_PIN, LOW);
  liftServo.write(liftAngle);
  liftAttached = true;
  return true;
}

static void liftTo(int angle) {
  if (!ensureLiftAttached()) return;

  if (angle < LIFT_MIN_ANGLE) angle = LIFT_MIN_ANGLE;
  if (angle > LIFT_MAX_ANGLE) angle = LIFT_MAX_ANGLE;

  // Step by 5 degrees, but finish exactly on the target so the measured
  // limits (24 and 54) are not rounded away.
  while (liftAngle != angle) {
    int next;

    if (liftAngle < angle) {
      next = liftAngle + SERVO_STEP_DEG;
      if (next > angle) next = angle;
    } else {
      next = liftAngle - SERVO_STEP_DEG;
      if (next < angle) next = angle;
    }

    liftAngle = next;
    liftServo.write(liftAngle);
    wifiServiceDelay(SERVO_STEP_DELAY_MS);
  }

  Serial.print("Lift -> ");
  Serial.print(liftAngle);
  Serial.println(" deg");
}

void initPickAndPlace() {
  pinMode(MAGNET_PIN, OUTPUT);
  setMagnet(false);

  // Do not attach or write the lift servo here. It attaches on the first
  // actual movement command, so flashing/resetting does not move it.
  liftAngle = LIFT_UP_ANGLE;
  liftAttached = false;
}

int liftAngleNow() {
  return liftAngle;
}

bool magnetIsOn() {
  return magnetOn;
}

// Move to the source angle, pick, move to the destination angle,
// release, then raise again.
static void performPickAndPlace(int sourceAngle, int destAngle) {
  Serial.print("PICK/PLACE START: source ");
  Serial.print(sourceAngle);
  Serial.print(" deg, destination ");
  Serial.print(destAngle);
  Serial.println(" deg");

  setMagnet(false);

  if (servoAngleNow() != sourceAngle) {
    moveServo(sourceAngle);
    wifiServiceDelay(PICK_MOVE_SETTLE_MS);
  }

  liftTo(LIFT_DOWN_ANGLE);
  wifiServiceDelay(LIFT_SETTLE_MS);

  setMagnet(true);
  wifiServiceDelay(MAGNET_PICK_MS);

  liftTo(LIFT_UP_ANGLE);
  wifiServiceDelay(LIFT_SETTLE_MS);

  moveServo(destAngle);

  // The lift must stay up for the whole trip. Reassert it after the arm
  // reaches the destination, then lower only at the destination angle.
  liftTo(LIFT_UP_ANGLE);
  wifiServiceDelay(PICK_MOVE_SETTLE_MS);

  liftTo(LIFT_DOWN_ANGLE);
  wifiServiceDelay(LIFT_SETTLE_MS);

  setMagnet(false);
  wifiServiceDelay(MAGNET_DROP_MS);

  liftTo(LIFT_UP_ANGLE);
  wifiServiceDelay(LIFT_SETTLE_MS);

  Serial.println("PICK/PLACE DONE");
}

// Pick at the current M2 position and place at PLACE_ANGLE.
static void runPickAndPlace() {
  performPickAndPlace(servoAngleNow(), PLACE_ANGLE);
}

// Same 5-degree rounding moveServo() applies.
static int servoGridAngle(int angle) {
  if (angle < SERVO_MIN_ANGLE) angle = SERVO_MIN_ANGLE;
  if (angle > SERVO_MAX_ANGLE) angle = SERVO_MAX_ANGLE;

  return ((angle + SERVO_STEP_DEG / 2) / SERVO_STEP_DEG) * SERVO_STEP_DEG;
}

// Commands look like "pickwr": pick white, place red.
// Color letters are w, g, r, b.
static bool parsePickPair(int& sourceIndex, int& destIndex) {
  const char* token = currentCommand();

  if (strncmp(token, "pick", 4) != 0) return false;
  if (token[4] == '\0' || token[5] == '\0' || token[6] != '\0') return false;

  int source = stripIndexFor(token[4]);
  int dest = stripIndexFor(token[5]);

  if (source < 0 || dest < 0) return false;

  sourceIndex = source;
  destIndex = dest;
  return true;
}

// Find both colors before picking up anything, so a missing destination
// aborts without carrying an object across the table.
static void runPickAndPlaceBetween(int sourceIndex, int destIndex) {
  Serial.print("COLOR PICK/PLACE: ");
  Serial.print(colors[sourceIndex].name);
  Serial.print(" -> ");
  Serial.println(colors[destIndex].name);

  setMagnet(false);
  liftTo(LIFT_UP_ANGLE);
  wifiServiceDelay(LIFT_SETTLE_MS);

  setRGB(255, 255, 255);
  wifiServiceDelay(SEEK_SETTLE_MS);

  Serial.print("Seeking source ");
  Serial.print(colors[sourceIndex].name);
  Serial.println("...");

  int sourceAngle = seekColor(sourceIndex);

  if (sourceAngle < 0) {
    Serial.print("No ");
    Serial.print(colors[sourceIndex].name);
    Serial.println(" found for pickup. Aborting.");
    return;
  }

  Serial.print(colors[sourceIndex].name);
  Serial.print(" source at ");
  Serial.print(sourceAngle);
  Serial.println(" deg");

  Serial.print("Seeking destination ");
  Serial.print(colors[destIndex].name);
  Serial.println("...");

  int destAngle = seekColor(destIndex);

  if (destAngle < 0) {
    Serial.print("No ");
    Serial.print(colors[destIndex].name);
    Serial.println(" found for drop-off. Aborting before pickup.");
    return;
  }

  Serial.print(colors[destIndex].name);
  Serial.print(" destination at ");
  Serial.print(destAngle);
  Serial.println(" deg");

  if (servoGridAngle(sourceAngle) == servoGridAngle(destAngle)) {
    Serial.println("Source and destination resolve to the same servo position. Aborting.");
    return;
  }

  performPickAndPlace(sourceAngle, destAngle);
}

// delta > 0 moves the lift up (toward 24 deg); delta < 0 moves it down.
static void liftJog(int delta) {
  if (!ensureLiftAttached()) return;

  int next = liftAngle - delta;

  if (next < LIFT_MIN_ANGLE) next = LIFT_MIN_ANGLE;
  if (next > LIFT_MAX_ANGLE) next = LIFT_MAX_ANGLE;

  if (next == liftAngle) {
    Serial.print("Lift at limit: ");
    Serial.print(liftAngle);
    Serial.println(" deg");
    return;
  }

  liftAngle = next;
  liftServo.write(liftAngle);
  wifiServiceDelay(SERVO_STEP_DELAY_MS);

  Serial.print("Lift -> ");
  Serial.print(liftAngle);
  Serial.println(" deg");
}

bool handlePickPlaceCommand() {
  if (isCommand("up", 'u')) {
    liftTo(LIFT_UP_ANGLE);
    return true;
  }

  if (isCommand("lower", 'l')) {
    liftTo(LIFT_DOWN_ANGLE);
    return true;
  }

  if (isCommand("+", '+')) {
    liftJog(LIFT_FINE_JOG_DEG);
    return true;
  }

  if (isCommand("-", '-')) {
    liftJog(-LIFT_FINE_JOG_DEG);
    return true;
  }

  if (isCommand("magnet", 'm')) {
    setMagnet(!magnetOn);
    return true;
  }

  int sourceIndex;
  int destIndex;

  if (parsePickPair(sourceIndex, destIndex)) {
    runPickAndPlaceBetween(sourceIndex, destIndex);
    return true;
  }

  if (isCommand("pick", 'p')) {
    runPickAndPlace();
    return true;
  }

  return false;
}

// ===== detection.cpp =====

int displayedColor = 0;  // WHITE
int candidateColor = -1;
int stableCount = 0;
float lastLdrReading = 0;

void startDetection(const char* source) {
  mode = MODE_DETECTION;

  // Fresh detection run: start on white, no candidate yet.
  displayedColor = 0;
  candidateColor = -1;
  stableCount = 0;
  lastLdrReading = 0;

  showColor(displayedColor);

  Serial.println();
  Serial.print("DETECTION STARTED using ");
  Serial.println(source);
  printDetectorTable();

  Serial.println("COLOR DETECTOR READY");
  Serial.println("Send c for the menu, x for the values in use.");
  Serial.println("Servo at M2: g/b/r/w = find that color");
  Serial.println("  a/d = jog, number = jump to that angle, v = scan");
  Serial.println("Lift at A0: u = up, l = lower, m = magnet, p = pick/place");
  Serial.println("Fine lift: + / - = 3 deg up/down");
}

void runDetection() {
  // Allow leaving detection, dumping the table, or moving the
  // servo without disturbing the measurement loop.
  if (readCommand()) {
    if (isCommand("menu", 'c')) {
      turnOffRGB();
      mode = MODE_MENU;
      printMenuPrompt();
      return;
    }

    if (isCommand("values", 'x')) {
      printDetectorTable();
      return;
    }

    if (handleServoUtilityCommand()) {
      return;
    }

    if (handlePickPlaceCommand()) {
      return;
    }

    if (seekForCommand()) {
      return;
    }
  }

  // LED stays on in its current color during measurement.
  float reading = readAverageLDR();
  lastLdrReading = reading;
  int detected = classifyReading(reading);

  if (detected == candidateColor) {
    stableCount++;
  } else {
    candidateColor = detected;
    stableCount = 1;
  }

  // Change LED only after three consecutive matching scans.
  if (stableCount >= REQUIRED_STABLE_SCANS &&
      detected != displayedColor) {

    displayedColor = detected;
    showColor(displayedColor);

    Serial.print("Detected color: ");
    Serial.println(colors[displayedColor].name);
  }

  Serial.print("LDR = ");
  Serial.print(reading, 2);

  Serial.print(" | Candidate = ");
  Serial.print(colors[detected].name);

  Serial.print(" | LED display = ");
  Serial.println(colors[displayedColor].name);

  delay(SCAN_DELAY_MS);
}

// ===== menu.cpp =====

Mode mode = MODE_MENU;

void printMenuPrompt() {
  Serial.println();
  Serial.println("Choose how to get calibration values:");
  Serial.println("  s = sense/calibrate strips now");
  Serial.println("  o = reuse the old saved values");
  Serial.println("Servo at M2: g/b/r/w = find that color");
  Serial.println("  a = jog left, d = jog right, or type an angle to jump there");
  Serial.println("  k = show limits, v = scan and print what the LDR sees");
  Serial.println("Lift at A0: u = up, l = lower, m = magnet, p = pick/place");
  Serial.println("Fine lift: + / - = 3 deg up/down");
}

void handleMenuInput() {
  if (!readCommand()) return;

  if (isCommand("sense", 's')) {
    // Start a fresh sensing run from the old saved values.
    loadOldValues();
    stripsCalibrated = 0;

    mode = MODE_CALIBRATION;

    Serial.println();
    Serial.println("SENSE/CALIBRATE MODE");
    printPrompt();
  }
  else if (isCommand("old", 'o')) {
    loadOldValues();
    startDetection("the old saved values");
  }
  else if (handleServoUtilityCommand()) {
    // Handled above.
  }
  else if (handlePickPlaceCommand()) {
    // Handled above.
  }
  else if (seekForCommand()) {
    // Handled above.
  }
  else {
    Serial.println("Invalid input. Enter s (sense/calibrate), o (old values),");
    Serial.println("or green, blue, red, white for the servo.");
  }
}

void handleCalibrationInput() {
  if (!readCommand()) return;

  if (isCommand("results", 'x')) {
    printAllResults();
    return;
  }

  if (isCommand("detect", 't')) {
    startDetection("the current calibration values");
    return;
  }

  // Jog, angle jump, limits and scan all work without leaving
  // calibration, so the arm can be positioned over each strip.
  if (handleServoUtilityCommand()) {
    return;
  }

  if (handlePickPlaceCommand()) {
    return;
  }

  char strip = 0;

  if (isCommand("white", 'w')) {
    strip = 'w';
  }
  else if (isCommand("blue", 'b')) {
    strip = 'b';
  }
  else if (isCommand("green", 'g')) {
    strip = 'g';
  }
  else if (isCommand("red", 'r')) {
    strip = 'r';
  }

  if (strip != 0) {
    if (stripsCalibrated >= 4) {
      Serial.println("All four strips calibrated.");
      Serial.println("Type x to print all results.");
      return;
    }

    calibrateStrip(strip);

    if (stripsCalibrated >= 4) {
      Serial.println();
      Serial.println("All four strips calibrated.");
      startDetection("the measured calibration");
    } else {
      printPrompt();
    }

    return;
  }

  Serial.println("Invalid input. Enter w, b, g, r, x, t, a/d, <, >, angle, k, or v.");
}

// ===== wifi_control.cpp =====

static const unsigned long WIFI_RETRY_MS = 10000;
static const unsigned long WIFI_IP_TIMEOUT_MS = 15000;
static const unsigned long HTTP_TIMEOUT_MS = 800;

static WiFiServer httpServer(80);

static bool wifiEnabled = false;
static bool serverStarted = false;
static bool waitingForIpPrinted = false;
static unsigned long lastWifiAttemptMs = 0;
static unsigned long connectedSinceMs = 0;

static const char* modeName() {
  switch (mode) {
    case MODE_MENU: return "MENU";
    case MODE_CALIBRATION: return "CALIBRATION";
    case MODE_DETECTION: return "DETECTION";
  }

  return "UNKNOWN";
}

static void sendHeaders(
  WiFiClient& client,
  const char* status,
  const char* contentType
) {
  client.print("HTTP/1.1 ");
  client.println(status);
  client.print("Content-Type: ");
  client.println(contentType);
  client.println("Cache-Control: no-store");
  client.println("Connection: close");
  client.println();
}

static void sendNoContent(WiFiClient& client) {
  client.println("HTTP/1.1 204 No Content");
  client.println("Connection: close");
  client.println();
  client.flush();
  client.stop();
}

static void sendJsonOk(WiFiClient& client) {
  sendHeaders(client, "200 OK", "application/json");
  client.println("{\"ok\":true}");
  client.flush();
  client.stop();
}

static void sendJsonError(
  WiFiClient& client,
  const char* status,
  const char* error
) {
  sendHeaders(client, status, "application/json");
  client.print("{\"ok\":false,\"error\":\"");
  client.print(error);
  client.println("\"}");
  client.flush();
  client.stop();
}

static void sendText(
  WiFiClient& client,
  const char* status,
  const char* body
) {
  sendHeaders(client, status, "text/plain");
  client.println(body);
  client.flush();
  client.stop();
}

static void sendStatus(WiFiClient& client) {
  sendHeaders(client, "200 OK", "application/json");

  client.print("{\"mode\":\"");
  client.print(modeName());
  client.print("\",\"m2\":");
  client.print(servoAngleNow());
  client.print(",\"strips\":");
  client.print(stripsCalibrated);
  client.print(",\"displayed\":\"");
  client.print(colors[displayedColor].name);
  client.print("\",\"candidate\":\"");

  if (candidateColor >= 0 && candidateColor < NUM_COLORS) {
    client.print(colors[candidateColor].name);
  } else {
    client.print("none");
  }

  client.print("\",\"ldr\":");
  client.print(lastLdrReading, 2);
  client.print(",\"lift\":");
  client.print(liftAngleNow());
  client.print(",\"magnet\":");
  client.print(magnetIsOn() ? "true" : "false");
  client.println("}");
  client.flush();
  client.stop();
}

static int hexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;

  return -1;
}

// Decode %XX escapes. A '+' stays a plus, so the '+' jog command works.
static bool decodeToken(const char* input, char* output, size_t outputSize) {
  size_t length = 0;

  while (*input != '\0' && *input != '&') {
    char c = *input++;

    if (c == '%') {
      if (input[0] == '\0' || input[1] == '\0') return false;

      int high = hexValue(input[0]);
      int low = hexValue(input[1]);

      if (high < 0 || low < 0) return false;

      c = (char)((high << 4) | low);
      input += 2;
    }

    if (c <= ' ' || c == 127) return false;
    if (length + 1 > outputSize) return false;

    output[length++] = c;
  }

  output[length] = '\0';

  return length > 0;
}

static const char HTTP_PAGE[] = R"PAGE(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Color Sorter</title>
<style>
body{font-family:system-ui,sans-serif;margin:1rem;max-width:42rem}
h1{font-size:1.4rem}
h2{font-size:1rem;margin:1rem 0 .4rem}
button{margin:.15rem;padding:.5rem .7rem;font-size:1rem}
select{padding:.45rem;font-size:1rem}
input{padding:.45rem;font-size:1rem;width:5rem}
pre{background:#eee;padding:.6rem;border-radius:.4rem;white-space:pre-wrap}
</style>
</head>
<body>
<h1>Color Sorter</h1>
<pre id="status">loading...</pre>

<h2>Mode</h2>
<button onclick="cmd('s')">Sense / calibrate</button>
<button onclick="cmd('o')">Old values + detect</button>
<button onclick="cmd('c')">Menu</button>

<h2>Servo</h2>
<button onclick="cmd('a')">&lt; Left</button>
<button onclick="cmd('d')">Right &gt;</button>
<button onclick="cmd('k')">Limits</button>
<button onclick="cmd('v')">Scan</button>
<input id="angle" type="number" min="0" max="120" value="90">
<button onclick="cmd(document.getElementById('angle').value)">Go</button>

<h2>Lift / magnet</h2>
<button onclick="cmd('u')">Lift up (24&deg;)</button>
<button onclick="cmd('l')">Lift down (54&deg;)</button>
<button onclick="cmd('+')">Up +3&deg;</button>
<button onclick="cmd('-')">Down -3&deg;</button>
<button onclick="cmd('m')">Magnet toggle</button>
<button onclick="cmd('p')">Pick here, place at 90&deg;</button>

<h2>Pick color &rarr; place color</h2>
<label>Pick from
<select id="sourceColor">
<option value="w">White</option>
<option value="g">Green</option>
<option value="r">Red</option>
<option value="b">Blue</option>
</select>
</label>
<label>Place at
<select id="destColor">
<option value="w">White</option>
<option value="g">Green</option>
<option value="r" selected>Red</option>
<option value="b">Blue</option>
</select>
</label>
<button onclick="pickBetween()">Find and move</button>

<h2>Calibration</h2>
<button onclick="cmd('w')">Cal W</button>
<button onclick="cmd('b')">Cal B</button>
<button onclick="cmd('g')">Cal G</button>
<button onclick="cmd('r')">Cal R</button>
<button onclick="cmd('x')">Results</button>
<button onclick="cmd('t')">Start detect</button>

<h2>Detection</h2>
<button onclick="cmd('w')">Seek W</button>
<button onclick="cmd('b')">Seek B</button>
<button onclick="cmd('g')">Seek G</button>
<button onclick="cmd('r')">Seek R</button>
<button onclick="cmd('x')">Values</button>

<h2>Any command</h2>
<input id="raw" type="text" placeholder="command">
<button onclick="cmd(document.getElementById('raw').value);document.getElementById('raw').value=''">Send</button>

<script>
function pickBetween() {
  const from = document.getElementById('sourceColor').value;
  const to = document.getElementById('destColor').value;
  cmd('pick' + from + to);
}
async function cmd(c) {
  if (!c) return;
  try {
    const r = await fetch('/cmd?c=' + encodeURIComponent(c), {cache: 'no-store'});
    const j = await r.json();
    if (!j.ok) alert(j.error || 'command rejected');
    setTimeout(refresh, 200);
  } catch (e) {
    alert('request failed');
  }
}
async function refresh() {
  try {
    const r = await fetch('/status', {cache: 'no-store'});
    const j = await r.json();
    document.getElementById('status').textContent =
      'Mode: ' + j.mode +
      '\nM2 servo: ' + j.m2 + ' deg' +
      '\nLift servo: ' + j.lift + ' deg' +
      '\nCalibrated strips: ' + j.strips +
      '\nLED: ' + j.displayed + ' | candidate: ' + j.candidate +
      '\nLDR: ' + j.ldr +
      '\nMagnet: ' + (j.magnet ? 'ON' : 'OFF');
  } catch (e) {
  }
}
refresh();
setInterval(refresh, 1000);
</script>
</body>
</html>
)PAGE";

static void handleHttpClient() {
  WiFiClient client = httpServer.available();

  if (!client) return;

  char request[192];
  int length = 0;
  bool complete = false;
  unsigned long started = millis();

  while (!complete && (millis() - started) < HTTP_TIMEOUT_MS) {
    while (client.available() > 0) {
      char c = (char)client.read();

      if (c == '\n') {
        complete = true;
        break;
      }

      if (c != '\r' && length < (int)sizeof(request) - 1) {
        request[length++] = c;
      }
    }
  }

  request[length] = '\0';

  if (!complete || length == 0) {
    client.stop();
    return;
  }

  if (strncmp(request, "GET ", 4) != 0) {
    sendText(client, "405 Method Not Allowed", "GET only");
    return;
  }

  char* path = request + 4;
  char* space = strchr(path, ' ');

  if (space != NULL) *space = '\0';

  if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
    sendHeaders(client, "200 OK", "text/html; charset=utf-8");
    client.print(HTTP_PAGE);
    client.flush();
    client.stop();
    return;
  }

  if (strcmp(path, "/status") == 0) {
    sendStatus(client);
    return;
  }

  if (strncmp(path, "/cmd", 4) == 0) {
    if (path[4] != '?' || strncmp(path + 5, "c=", 2) != 0) {
      sendJsonError(client, "400 Bad Request", "missing c parameter");
      return;
    }

    char token[CMD_BUF_SIZE];

    if (!decodeToken(path + 7, token, sizeof(token))) {
      sendJsonError(client, "400 Bad Request", "invalid command");
      return;
    }

    if (!submitCommand(token)) {
      sendJsonError(client, "409 Conflict", "busy");
      return;
    }

    sendJsonOk(client);
    return;
  }

  if (strcmp(path, "/favicon.ico") == 0) {
    sendNoContent(client);
    return;
  }

  sendText(client, "404 Not Found", "not found");
}

void wifiControlBegin() {
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("WiFi: module not found; serial control only.");
    return;
  }

  if (SECRET_SSID[0] == '\0') {
    Serial.println("WiFi: set SECRET_SSID and SECRET_PASS at the top of this sketch.");
    return;
  }

  wifiEnabled = true;
  lastWifiAttemptMs = millis();

  Serial.print("WiFi: connecting to ");
  Serial.println(SECRET_SSID);

  WiFi.begin(SECRET_SSID, SECRET_PASS);
}

void wifiControlPoll() {
  if (!wifiEnabled) return;
  if (WiFi.status() != WL_CONNECTED) {
    if (serverStarted) {
      httpServer.end();
      serverStarted = false;
      Serial.println("WiFi: disconnected.");
    }

    waitingForIpPrinted = false;
    connectedSinceMs = 0;

    if (millis() - lastWifiAttemptMs >= WIFI_RETRY_MS) {
      lastWifiAttemptMs = millis();

      Serial.print("WiFi: retrying ");
      Serial.println(SECRET_SSID);

      WiFi.begin(SECRET_SSID, SECRET_PASS);
    }

    return;
  }

  IPAddress ip = WiFi.localIP();

  // The module can report WL_CONNECTED before DHCP has handed out an
  // address. Do not start the server on 0.0.0.0.
  if ((uint32_t)ip == 0) {
    if (serverStarted) {
      httpServer.end();
      serverStarted = false;
    }

    if (!waitingForIpPrinted) {
      waitingForIpPrinted = true;
      connectedSinceMs = millis();
      Serial.println("WiFi: connected, waiting for an IP address...");
    }

    if (millis() - connectedSinceMs >= WIFI_IP_TIMEOUT_MS) {
      Serial.println("WiFi: no IP address, reconnecting...");

      waitingForIpPrinted = false;
      connectedSinceMs = 0;
      lastWifiAttemptMs = millis();

      WiFi.disconnect();
      WiFi.begin(SECRET_SSID, SECRET_PASS);
    }

    return;
  }

  if (!serverStarted) {
    httpServer.begin();
    serverStarted = true;
    waitingForIpPrinted = false;

    Serial.print("WiFi: open http://");
    Serial.println(ip);
  }

  handleHttpClient();
}

void wifiServiceDelay(unsigned long ms) {
  unsigned long start = millis();

  while (millis() - start < ms) {
    wifiControlPoll();
    delay(5);
  }
}

// ===== arduino_maintain.ino =====

void setup() {
  Serial.begin(115200);

  pinMode(LDR_PIN, INPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  turnOffRGB();
  loadOldValues();
  initServo();
  initPickAndPlace();

  Serial.println("RGB + LDR CALIBRATION + COLOR DETECTOR");
  Serial.println("Arduino Uno R4 WiFi");
  Serial.println("Sensing averages 50 LDR samples per light.");

  printMenuPrompt();
  wifiControlBegin();
}

void loop() {
  wifiControlPoll();

  if (mode == MODE_MENU) {
    handleMenuInput();
  }
  else if (mode == MODE_CALIBRATION) {
    handleCalibrationInput();
  }
  else {
    runDetection();
  }
}
