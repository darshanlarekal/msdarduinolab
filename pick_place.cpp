#include "pick_place.h"

#include <Arduino.h>
#include <Servo.h>
#include <string.h>

#include "color_data.h"
#include "command_input.h"
#include "config.h"
#include "servo_arm.h"
#include "wifi_control.h"

static Servo liftServo;
static int liftAngle = LIFT_UP_ANGLE;
static bool magnetOn = false;
static bool liftAttached = false;

void setMagnet(bool on) {
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

bool liftTo(int angle) {
  if (!ensureLiftAttached()) return false;

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
  return true;
}

void initPickAndPlace() {
  pinMode(MAGNET_PIN, OUTPUT);
  if (OBJECT_SENSOR_ENABLED) {
    pinMode(OBJECT_SENSOR_PIN, INPUT_PULLUP);
  }
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

bool objectDetected() {
  if (!OBJECT_SENSOR_ENABLED) return false;

  int state = digitalRead(OBJECT_SENSOR_PIN);

  if (OBJECT_SENSOR_ACTIVE_LOW) {
    return state == LOW;
  }

  return state == HIGH;
}

bool lowerUntilObject() {
  if (!OBJECT_SENSOR_ENABLED) {
    Serial.println("Object sensor disabled; lowering to down limit.");
    liftTo(LIFT_DOWN_ANGLE);
    return false;
  }

  if (!liftTo(LIFT_UP_ANGLE)) return false;

  if (objectDetected()) {
    Serial.println("Object already sensed at the up position.");
    return true;
  }

  for (int angle = LIFT_UP_ANGLE;
       angle <= LIFT_DOWN_ANGLE;
       angle += SERVO_STEP_DEG) {

    if (!liftTo(angle)) return false;
    wifiServiceDelay(OBJECT_SETTLE_MS);

    if (objectDetected()) {
      Serial.print("Object sensed at lift ");
      Serial.print(angle);
      Serial.println(" deg.");
      return true;
    }
  }

  Serial.println("Object sensor did not trigger; using down limit.");
  return false;
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
