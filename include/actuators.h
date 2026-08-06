/*
 * Chamber Master - Actuators Module (PWM Fan + Transistor Hard Kill, Servo Vent, LED)
 */

#ifndef ACTUATORS_H
#define ACTUATORS_H

#include "config.h"

void initActuators();
void setFanDuty(uint8_t d, bool allowBelow20 = false);
void updateFan(FanSpeed s);
void startOpenVent(bool half = false);
void startCloseVent();
void processVentState();
void processStartupVent();
void updateLED();

#endif // ACTUATORS_H
