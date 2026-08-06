/*
 * Chamber Master - Sensors Module Implementation
 */

#include "sensors.h"

// Fan Tachometer Interrupt with 2000µs Microsecond Debouncing (Max 15,000 RPM)
void IRAM_ATTR fanPulseISR() {
  unsigned long nowUs = micros();
  if (nowUs - lastFanPulseUs >= 2000) {
    fanPulseCount++;
    lastFanPulseUs = nowUs;
  }
}

void initSensors() {
  chamberSensor.begin();
  intakeSensor.begin();
  chamberSensor.setResolution(10);
  intakeSensor.setResolution(10);
  chamberSensor.setWaitForConversion(false);
  intakeSensor.setWaitForConversion(false);
  dht.begin();

  pinMode(FAN_TACH_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FAN_TACH_PIN), fanPulseISR, FALLING);
}

void requestTempsNonBlocking() {
  chamberSensor.requestTemperatures();
  intakeSensor.requestTemperatures();
  tempsRequested = true;
  lastTempRequestMs = millis();
}

void readTempsAfterDelay() {
  float c = chamberSensor.getTempCByIndex(0);
  float i = intakeSensor.getTempCByIndex(0);
  chamberTemp = (c == -127.0f || c == 85.0f) ? NAN : c;
  intakeTemp = (i == -127.0f || i == 85.0f) ? NAN : i;
  tempsRequested = false;
  displayNeedsUpdate = true;
}

void processSensors() {
  unsigned long now = millis();

  // DHT11 Ambient Temperature & Humidity Reading
  if (now - lastDhtReadMs >= DHT_MIN_INTERVAL_MS) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && t > -40 && t < 80) {
      ambientTemp = t;
      ambientHum = isnan(h) ? NAN : roundf(h);
      displayNeedsUpdate = true;
    }
    lastDhtReadMs = now;
  }

  // Non-blocking OneWire DS18B20 Temp Conversion State Machine
  if (!tempsRequested && now - lastTempRequestMs >= 1000) {
    requestTempsNonBlocking();
  } else if (tempsRequested && now - lastTempRequestMs >= DS_CONV_MS) {
    readTempsAfterDelay();
  }
}

void processRpmMeasurement() {
  unsigned long now = millis();
  if (now - lastRpmMs >= RPM_SAMPLE_MS) {
    if (fanDutyCycle == 0 || fanSpeed == FAN_OFF || digitalRead(FAN_POWER_PIN) == LOW) {
      currentFanRPM = 0.0f;
      noInterrupts();
      fanPulseCount = 0;
      interrupts();
    } else {
      noInterrupts();
      unsigned long p = fanPulseCount;
      fanPulseCount = 0;
      interrupts();
      unsigned long elapsedMs = now - lastRpmMs;
      currentFanRPM = (p > 0) ? (p * 60.0f * 1000.0f) / (elapsedMs * 2.0f) : 0.0f;
    }
    lastRpmMs = now;
    displayNeedsUpdate = true;
  }
}
