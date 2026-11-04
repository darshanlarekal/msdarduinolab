#include "pick_place.h"

#include <Arduino.h>
#include <Servo.h>

#include "config.h"
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
