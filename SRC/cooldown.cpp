/*
 * Chamber Master - Cooldown Logic Module Implementation
 */

#include "cooldown.h"
#include "actuators.h"

void startAdaptiveCooldown() {
  activeMode = 6;
  inSubMenu = true;
  activeTarget = -2.0f;
  prefs.putInt("activeMode", activeMode);

  cooldownStartMs = cooldownLastCheckMs = millis();
  cooldownStartTemp = isnan(chamberTemp) ? 60.0f : chamberTemp;
  cooldownLastTemp = cooldownStartTemp;
  cooldownFanDuty = NORMAL_MIN_DUTY; // Start at 20% fan duty
  cooldownEstSeconds = -1;
  cooldownProgress = 0.0f;

  startOpenVent(false);  // Open vent first to avoid back-pressure
  setFanDuty(cooldownFanDuty, false); // Then engage fan at min 20% duty
  displayNeedsUpdate = true;
}

void processAdaptiveCooldown() {
  unsigned long now = millis();
  float target = isnan(ambientTemp) ? 25.0f : ambientTemp + COOLDOWN_TARGET_OFFSET;
  bool cooldownFinished = (chamberTemp <= target + 0.5f);

  if (cooldownLastCheckMs == 0 || isnan(cooldownLastTemp)) {
    cooldownLastCheckMs = now;
    cooldownLastTemp = chamberTemp;
  }

  // Keep vent open during active cooling
  if (!cooldownFinished && ventState != VENT_OPEN && ventState != VENT_OPENING) {
    startOpenVent(false);
  }

  // Smooth 1-second countdown tick for remaining time estimate
  if (cooldownEstSeconds > 0 && now - lastCountdownSecMs >= 1000) {
    cooldownEstSeconds--;
    lastCountdownSecMs = now;
    displayNeedsUpdate = true;
  }

  // 60-second adaptive rate adjustment sample window
  if (now - cooldownLastCheckMs >= COOLDOWN_SAMPLE_MS) {
    float drop = cooldownLastTemp - chamberTemp;

    if (cooldownFinished) {
      setFanDuty(0, true); // Hard kill fan (cuts 2N2222 GND)
      startCloseVent();
      cooldownEstSeconds = 0;
      cooldownProgress = 1.0f;
      displayNeedsUpdate = true;
    } else {
      float dpm = drop * (60000.0f / (now - cooldownLastCheckMs));
      int8_t adj = (dpm < COOLDOWN_RATE_DEG_PER_MIN - 0.3f) ? 20 :
                   (dpm > COOLDOWN_RATE_DEG_PER_MIN + 0.3f) ? -20 : 0;

      // Enforce minimum 20% fan duty (NORMAL_MIN_DUTY) while cooling active
      cooldownFanDuty = constrain(cooldownFanDuty + adj, NORMAL_MIN_DUTY, 255);
      setFanDuty(cooldownFanDuty, false);

      // Track progress from original start temperature baseline
      float total = cooldownStartTemp - target;
      float currentDrop = cooldownStartTemp - chamberTemp;
      cooldownProgress = (total > 0.5f) ? constrain(currentDrop / total, 0.0f, 1.0f) : 1.0f;

      if (drop > 0.1f && total > 0.5f && (now - cooldownLastCheckMs) > 1000) {
        float rate = drop * (60000.0f / (now - cooldownLastCheckMs));
        if (rate > 0.1f) {
          float rem = chamberTemp - target;
          if (rem > 0) {
            cooldownEstSeconds = (long)(rem / rate * 60.0f);
            lastCountdownSecMs = now;
          }
        }
      }
    }

    cooldownLastTemp = chamberTemp;
    cooldownLastCheckMs = now;
  }
}
