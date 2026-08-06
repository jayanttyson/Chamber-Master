/*
 * 3D Printer Enclosure Controller - Version 2.5 (Modular Production Firmware)
 * 
 * Hardware Rationale (Why Hard Kill Transistor is Needed):
 * Standard 4-pin PC cooling fans adhere to the Intel 4-Wire PWM Fan Specification.
 * As an integrated CPU safety feature, many PC fans refuse to stop when PWM duty
 * is set to 0% (falling back to a default ~20% min RPM curve so CPUs do not overheat
 * if PWM control fails). The 2N2222 low-side transistor on GPIO 15 physically cuts 
 * the fan's Ground (GND) line when FAN_OFF is commanded, ensuring true 0 RPM operation.
 * 
 * Modular Architecture:
 * - config.h / config.cpp           : Pin mappings, constants, shared state
 * - sensors.h / sensors.cpp         : DS18B20 OneWire, DHT11, Tachometer ISR & RPM logic
 * - actuators.h / actuators.cpp     : Fan PWM + 2N2222 GND hard kill, Servo Vent, LED
 * - cooldown.h / cooldown.cpp       : Adaptive thermal cooldown routine
 * - ui_display.h / ui_display.cpp   : OLED Display, Menu System, EC11 Encoder & Button
 * - web_dashboard.h / web_dashboard.cpp : HTML Web UI, REST JSON API, mDNS
 * 
 * Author: Jayant Bhatia
 * Version: 2.5
 * Date: August 2026
 * License: MIT License
 */

#define DEBUG // Comment this line to disable Serial debug output

#include "config.h"
#include "sensors.h"
#include "actuators.h"
#include "cooldown.h"
#include "ui_display.h"
#include "web_dashboard.h"

void setup() {
  #ifdef DEBUG
  Serial.begin(115200);
  delay(50);
  Serial.println("Starting 3D Printer Enclosure Controller v2.5 (Modular)...");
  #endif

  // Initialize display and read persistent preferences
  ensureOLED();
  prefs.begin("chamber_prefs", false);
  activeMode = prefs.getInt("activeMode", 0);
  if (activeMode < 0 || activeMode >= MENU_LEN) activeMode = 0;
  customTarget = prefs.getFloat("customTarget", 30.0f);
  if (customTarget < 0 || customTarget > 120) customTarget = 30.0f;
  prefs.putFloat("customTarget", customTarget);
  activeTarget = (activeMode == 5) ? customTarget : menuTargets[activeMode];
  menuIndex = activeMode;
  inSubMenu = true;

  // Initialize Hardware Modules
  initSensors();
  initActuators();
  initUI();
  setupWiFiAndServer();

  // Kick off non-blocking temperature sampling
  requestTempsNonBlocking();
  lastDhtReadMs = millis() - DHT_MIN_INTERVAL_MS;
  lastRpmMs = millis();

  drawSubMenu();
}

void loop() {
  // Always service web requests even during startup calibration
  serviceWebClient();

  // Process startup vent calibration sequence
  if (!startupVentDone) {
    processStartupVent();
    yield();
    return;
  }

  unsigned long now = millis();

  // Process Fan Tachometer & RPM
  processRpmMeasurement();

  // Process Rotary Encoder & Push Button User Input
  processUIInput();

  // Process Temperature & Humidity Sensors Non-Blocking
  processSensors();

  // Thermal Control Routine & Safety Management
  if (inSubMenu && !isnan(chamberTemp)) {
    // Safety check: Intake fault overrides all normal operation
    if (!intakeFault && !isnan(intakeTemp) && intakeTemp > chamberTemp + 5.0f) {
      intakeFault = true;
      startOpenVent(false);
      updateFan(FAN_HIGH);
      displayNeedsUpdate = true;
    } else if (intakeFault && !isnan(intakeTemp) && intakeTemp <= chamberTemp + 2.0f) {
      intakeFault = false;
      displayNeedsUpdate = true;
    }

    if (intakeFault) {
      updateFan(FAN_HIGH);
      startOpenVent(false);
    } else if (activeMode == 6) {
      // COOLDOWN MODE
      processAdaptiveCooldown();
    } else {
      // NORMAL PRESET MODES
      if (ventState == VENT_CLOSED) {
        if (chamberTemp > activeTarget + HYSTERESIS_TO_FULL) {
          startOpenVent(false);
          updateFan(FAN_HIGH);
        } else if (chamberTemp > activeTarget + HYSTERESIS_TO_HALF) {
          startOpenVent(true);
          updateFan(FAN_LOW);
        }
      } else if (ventState == VENT_HALF_OPEN || ventState == VENT_HALF_OPENING) {
        if (chamberTemp < activeTarget + HYSTERESIS_TO_CLOSED) {
          startCloseVent();
          updateFan(FAN_OFF);
        } else if (chamberTemp > activeTarget + HYSTERESIS_TO_FULL) {
          startOpenVent(false);
          updateFan(FAN_HIGH);
        }
      } else if (ventState == VENT_OPEN || ventState == VENT_OPENING) {
        if (chamberTemp <= activeTarget + HYSTERESIS_FROM_FULL) {
          startOpenVent(true);
          updateFan(FAN_LOW);
        }
      }
    }
  } else {
    updateFan(FAN_OFF);
    startCloseVent();
  }

  // Update actuators & status indicators
  processVentState();
  updateLED();
  yield();

  // Refresh OLED Display if needed
  renderDisplayIfNeeded();
}
