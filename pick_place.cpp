#include "pick_place.h"

#include <Arduino.h>
#include <Servo.h>

#include "command_input.h"
#include "config.h"
#include "servo_arm.h"
#include "wifi_control.h"

static Servo liftServo;
static int liftAngle = LIFT_UP_ANGLE;
static bool magnetOn = false;

static void setMagnet(bool on) {
  magnetOn = on;
  digitalWrite(MAGNET_PIN, on == MAGNET_ACTIVE_HIGH ? HIGH : LOW);

  Serial.print("Magnet ");
  Serial.println(on ? "ON" : "OFF");
}

static void liftTo(int angle) {
  if (angle < LIFT_MIN_ANGLE) angle = LIFT_MIN_ANGLE;
  if (angle > LIFT_MAX_ANGLE) angle = LIFT_MAX_ANGLE;

  // Keep every commanded position on the 5-degree grid.
  angle = ((angle + SERVO_STEP_DEG / 2) / SERVO_STEP_DEG) * SERVO_STEP_DEG;

  if (angle < LIFT_MIN_ANGLE) angle = LIFT_MIN_ANGLE;
  if (angle > LIFT_MAX_ANGLE) angle = LIFT_MAX_ANGLE;

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

void attachPickAndPlace() {
  pinMode(MAGNET_PIN, OUTPUT);
  setMagnet(false);

  if (liftServo.attach(LIFT_SERVO_PIN) == 0) {
    Serial.println("Lift servo attach failed on A0.");
  }

  // Send the boot position once; liftTo() would skip the write because
  // liftAngle already starts at the up angle.
  liftAngle = LIFT_UP_ANGLE;
  liftServo.write(liftAngle);

  Serial.print("Lift -> ");
  Serial.print(liftAngle);
  Serial.println(" deg");
}

int liftAngleNow() {
  return liftAngle;
}

bool magnetIsOn() {
  return magnetOn;
}

// Pick at the current M2 position, lift, rotate M2 to PLACE_ANGLE,
// lower, release, then raise again.
static void runPickAndPlace() {
  Serial.println("PICK/PLACE START");

  setMagnet(false);

  liftTo(LIFT_DOWN_ANGLE);
  wifiServiceDelay(LIFT_SETTLE_MS);

  setMagnet(true);
  wifiServiceDelay(MAGNET_PICK_MS);

  liftTo(LIFT_UP_ANGLE);
  wifiServiceDelay(LIFT_SETTLE_MS);

  int placeAngle = PLACE_ANGLE;

  if (placeAngle < SERVO_MIN_ANGLE) placeAngle = SERVO_MIN_ANGLE;
  if (placeAngle > SERVO_MAX_ANGLE) placeAngle = SERVO_MAX_ANGLE;

  moveServo(placeAngle);
  wifiServiceDelay(PICK_MOVE_SETTLE_MS);

  liftTo(LIFT_DOWN_ANGLE);
  wifiServiceDelay(LIFT_SETTLE_MS);

  setMagnet(false);
  wifiServiceDelay(MAGNET_DROP_MS);

  liftTo(LIFT_UP_ANGLE);
  wifiServiceDelay(LIFT_SETTLE_MS);

  Serial.println("PICK/PLACE DONE");
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

  if (isCommand("magnet", 'm')) {
    setMagnet(!magnetOn);
    return true;
  }

  if (isCommand("pick", 'p')) {
    runPickAndPlace();
    return true;
  }

  return false;
}
