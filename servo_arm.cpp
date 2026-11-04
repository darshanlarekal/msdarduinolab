#include "servo_arm.h"

#include <Arduino.h>
#include <Servo.h>

#include "config.h"
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
