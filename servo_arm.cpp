#include "servo_arm.h"

#include <Arduino.h>
#include <Servo.h>

#include "color_data.h"
#include "command_input.h"
#include "config.h"
#include "detection.h"
#include "sensing.h"
#include "wifi_control.h"

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
