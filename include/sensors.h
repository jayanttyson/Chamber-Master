/*
 * Chamber Master - Sensors Module (DS18B20, DHT11, Tachometer ISR)
 */

#ifndef SENSORS_H
#define SENSORS_H

#include "config.h"

void IRAM_ATTR fanPulseISR();
void initSensors();
void requestTempsNonBlocking();
void readTempsAfterDelay();
void processSensors();
void processRpmMeasurement();

#endif // SENSORS_H
