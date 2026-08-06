/*
 * Chamber Master - Actuators Module Implementation
 */

#include "actuators.h"

void initActuators() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(FAN_POWER_PIN, OUTPUT);
  digitalWrite(FAN_POWER_PIN, LOW); // Transistor cuts GND rail initially

  pinMode(FAN_PIN, OUTPUT);
  fanPWM.attachPin(FAN_PIN, FAN_PWM_FREQ, FAN_PWM_RES);
  fanPWM.writeScaled(0.0f);

  ventServo.attach(SERVO_PIN, 500, 2500);
  ventServo.setPeriodHertz(50);
  ventServo.writeMicroseconds(SERVO_STOP_PULSE);
  ventState = VENT_CLOSED;
  startupVentState = VENT_START_CLOSING;
  ventServo.writeMicroseconds(SERVO_FORWARD_PULSE);
  startupVentTimer = millis();
}

void setFanDuty(uint8_t d, bool allowBelow20) {
  fanDutyCycle = d;

  if (allowBelow20 && d < NORMAL_MIN_DUTY) {
    digitalWrite(FAN_POWER_PIN, LOW); // Low-side 2N2222 transistor cuts GND rail to override CPU fan 20% failsafe
    fanPWM.writeScaled(0.0f);
  } else {
    digitalWrite(FAN_POWER_PIN, HIGH); // Ground rail engaged
    uint8_t effectiveDuty = allowBelow20 ? d : max(d, NORMAL_MIN_DUTY);
    fanPWM.writeScaled(effectiveDuty / 255.0f);
  }

  displayNeedsUpdate = true;
}

void updateFan(FanSpeed s) {
  if (s != fanSpeed) {
    fanSpeed = s;
    uint8_t duty = (s == FAN_HIGH) ? 255 : (s == FAN_LOW) ? 140 : 0;
    setFanDuty(duty, (s == FAN_OFF));  // True 0 Hard Kill when OFF
  }
}

void startOpenVent(bool half) {
  bool open = false;
  int pulse = SERVO_REVERSE_PULSE;
  unsigned long t = half ? SERVO_HALF_OPEN_TIME : SERVO_OPEN_TIME;

  if (half) {
    if (ventState == VENT_CLOSED || ventState == VENT_OPENING || ventState == VENT_CLOSING) open = true;
    else if (ventState == VENT_OPEN) {
      open = true;
      pulse = SERVO_FORWARD_PULSE;
      t = SERVO_FULL_TO_HALF_TIME;
    }
  } else if (ventState == VENT_CLOSED || ventState == VENT_HALF_OPEN || ventState == VENT_HALF_OPENING || ventState == VENT_CLOSING) open = true;

  if (open) {
    ventServo.writeMicroseconds(SERVO_STOP_PULSE);
    ventState = half ? VENT_HALF_OPENING : VENT_OPENING;
    ventActionStartMs = millis();
    ventServo.writeMicroseconds(pulse);
    displayNeedsUpdate = true;
  }
}

void startCloseVent() {
  if (ventState == VENT_OPEN || ventState == VENT_HALF_OPEN || ventState == VENT_OPENING || ventState == VENT_HALF_OPENING) {
    ventServo.writeMicroseconds(SERVO_STOP_PULSE);
    ventState = VENT_CLOSING;
    ventActionStartMs = millis();
    ventServo.writeMicroseconds(SERVO_FORWARD_PULSE);
    displayNeedsUpdate = true;
  }
}

void processVentState() {
  unsigned long now = millis();
  unsigned long t = (ventState == VENT_HALF_OPENING && ventServo.readMicroseconds() == SERVO_FORWARD_PULSE) ? SERVO_FULL_TO_HALF_TIME :
                    (ventState == VENT_HALF_OPENING ? SERVO_HALF_OPEN_TIME : (ventState == VENT_OPENING ? SERVO_OPEN_TIME : SERVO_CLOSE_TIME));

  if (ventState == VENT_OPENING && now - ventActionStartMs >= SERVO_OPEN_TIME) {
    ventServo.writeMicroseconds(SERVO_STOP_PULSE);
    ventState = VENT_OPEN;
    displayNeedsUpdate = true;
  } else if (ventState == VENT_HALF_OPENING && now - ventActionStartMs >= t) {
    ventServo.writeMicroseconds(SERVO_STOP_PULSE);
    ventState = VENT_HALF_OPEN;
    displayNeedsUpdate = true;
  } else if (ventState == VENT_CLOSING && now - ventActionStartMs >= SERVO_CLOSE_TIME) {
    ventServo.writeMicroseconds(SERVO_STOP_PULSE);
    ventState = VENT_CLOSED;
    displayNeedsUpdate = true;
  }
}

void processStartupVent() {
  if (startupVentDone) return;

  unsigned long now = millis();
  switch (startupVentState) {
    case VENT_START_CLOSING:
      if (now - startupVentTimer >= SERVO_CLOSE_TIME) {
        ventServo.writeMicroseconds(SERVO_STOP_PULSE);
        startupVentState = VENT_START_WAIT_CLOSED;
        startupVentTimer = now;
      }
      break;
    case VENT_START_WAIT_CLOSED:
      if (now - startupVentTimer >= 1000) {
        ventServo.writeMicroseconds(SERVO_REVERSE_PULSE);
        startupVentState = VENT_START_OPENING;
        startupVentTimer = now;
      }
      break;
    case VENT_START_OPENING:
      if (now - startupVentTimer >= SERVO_OPEN_TIME) {
        ventServo.writeMicroseconds(SERVO_STOP_PULSE);
        startupVentState = VENT_START_WAIT_OPEN;
        startupVentTimer = now;
      }
      break;
    case VENT_START_WAIT_OPEN:
      if (now - startupVentTimer >= 1000) {
        ventServo.writeMicroseconds(SERVO_FORWARD_PULSE);
        startupVentState = VENT_START_CLOSING_AGAIN;
        startupVentTimer = now;
      }
      break;
    case VENT_START_CLOSING_AGAIN:
      if (now - startupVentTimer >= SERVO_CLOSE_TIME) {
        ventServo.writeMicroseconds(SERVO_STOP_PULSE);
        startupVentState = VENT_START_WAIT_CLOSED_AGAIN;
        startupVentTimer = now;
      }
      break;
    case VENT_START_WAIT_CLOSED_AGAIN:
      if (now - startupVentTimer >= 1000) {
        ventServo.writeMicroseconds(SERVO_STOP_PULSE);
        ventState = VENT_CLOSED;
        startupVentDone = true;
        displayNeedsUpdate = true;
      }
      break;
  }
}

void updateLED() {
  bool on = (fanSpeed != FAN_OFF) && (ventState == VENT_OPEN || ventState == VENT_HALF_OPEN || ventState == VENT_OPENING || ventState == VENT_HALF_OPENING);
  if (on != ledOn) {
    ledOn = on;
    digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
  }
}
