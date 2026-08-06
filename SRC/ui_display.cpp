/*
 * Chamber Master - UI & Display Module Implementation
 */

#include "ui_display.h"
#include "actuators.h"
#include "cooldown.h"

void ensureOLED() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    #ifdef DEBUG
    Serial.println(F("SSD1306 allocation failed"));
    #endif
    for (;;) delay(1000);
  }
  display.clearDisplay();
  display.display();
}

void initUI() {
  ensureOLED();
  pinMode(ENCODER_BTN, INPUT_PULLUP);
  encoder.attachHalfQuad(ENCODER_CLK, ENCODER_DT);
  encoder.setCount(0);
  lastEncoderCount = encoder.getCount();
}

void drawChamberTempBlink() {
  if (millis() - lastBlinkMs >= BLINK_INTERVAL_MS) {
    lastBlinkMs = millis();
    chamberTempVisible = !chamberTempVisible;
    displayNeedsUpdate = true;
  }
  display.setTextSize(1);
  display.setTextColor(chamberTempVisible ? SSD1306_WHITE : SSD1306_BLACK);
  display.setCursor(108, 0);
  display.printf("%02d", isnan(chamberTemp) ? 0 : (int)round(chamberTemp));
}

void drawMainMenu() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  int16_t x1, y1;
  uint16_t w, h;

  if (menuIndex == 6) {
    display.setTextSize(2);
    display.getTextBounds("COOLDOWN", 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2);
    display.print("COOLDOWN");
  } else if (menuIndex == 7) {
    if (WiFi.status() == WL_CONNECTED) {
      String url = "http://enclosure-monitor.local";
      qrcode.setScale(2);
      qrcode.draw(url.c_str(), 46, 8);
      const char* txt = "enclosure-monitor.local";
      display.setTextSize(1);
      display.getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
      display.setCursor((SCREEN_WIDTH - w) / 2, 2);
      display.print(txt);
    } else {
      display.setTextSize(1);
      display.getTextBounds("WiFi Disconnected", 0, 0, &x1, &y1, &w, &h);
      display.setCursor((SCREEN_WIDTH - w) / 2, 28);
      display.print("WiFi Disconnected");
    }
  } else {
    display.setTextSize(3);
    display.getTextBounds(menuItems[menuIndex], 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2);
    display.print(menuItems[menuIndex]);
    display.setTextSize(1);
    display.setCursor((SCREEN_WIDTH - 80) / 2, (SCREEN_HEIGHT - h) / 2 + h + 6);
    display.printf("Target: %.1fC", menuTargets[menuIndex] == -1.0f ? customTarget : menuTargets[menuIndex]);
  }
  drawChamberTempBlink();
  display.display();
  displayNeedsUpdate = false;
}

void drawCooldownScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 0);
  display.print("COOLING DOWN");
  display.setCursor(0, 12);
  display.printf("Fan: %3d%% (RPM: %4d)", (fanDutyCycle * 100) / 255, (int)currentFanRPM);

  display.setCursor(0, 24);
  display.print("Time: ");
  if (cooldownEstSeconds == 0) {
    display.print("Cooled");
  } else if (cooldownEstSeconds < 0) {
    display.print("Calculating...");
  } else if (cooldownEstSeconds >= 3600) {
    display.printf("%dh", cooldownEstSeconds / 3600);
  } else if (cooldownEstSeconds >= 120) {
    display.printf("%dm", cooldownEstSeconds / 60);
  } else {
    int mins = cooldownEstSeconds / 60;
    int secs = cooldownEstSeconds % 60;
    display.printf("%dm%02ds", mins, secs);
  }

  display.setCursor(0, 36);
  display.printf("Amb: %sC %d%%", isnan(ambientTemp) ? "--.-" : String(ambientTemp, 1).c_str(), isnan(ambientHum) ? 0 : (int)ambientHum);

  int barY = 56, barWidth = 120, barHeight = 6;
  display.drawRect(3, barY, barWidth, barHeight, SSD1306_WHITE);
  int fill = (int)(cooldownProgress * (barWidth - 2));
  if (fill > 0) display.fillRect(4, barY + 1, fill, barHeight - 2, SSD1306_WHITE);

  drawChamberTempBlink();
  display.display();
  displayNeedsUpdate = false;
}

void drawSubMenu() {
  if (activeMode == 6) {
    drawCooldownScreen();
    return;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.printf("Chamber: %sC", isnan(chamberTemp) ? "--.-" : String(chamberTemp, 1).c_str());
  display.setCursor(0, 12);
  display.printf("Intake : %sC", isnan(intakeTemp) ? "--.-" : String(intakeTemp, 1).c_str());
  display.setCursor(0, 24);
  display.printf("Ambient: %sC %d%%", isnan(ambientTemp) ? "--.-" : String(ambientTemp, 1).c_str(), isnan(ambientHum) ? 0 : (int)ambientHum);
  display.setCursor(0, 38);
  display.printf(activeMode == 5 ? "Target: %.1fC *CUST" : "Target: %.1fC (%s)", activeTarget, menuItems[activeMode]);
  display.setCursor(0, 52);
  display.printf("Fan:%3d%% Vent:%s", (fanDutyCycle * 100) / 255,
                 ventState == VENT_OPEN ? "OPEN" : (ventState == VENT_HALF_OPEN ? "HALF" : (ventState == VENT_CLOSED ? "CLOSED" : "MOV")));
  if (intakeFault) {
    display.setCursor(0, 0);
    display.print("INTAKE FAULT!");
  }
  drawChamberTempBlink();
  display.display();
  displayNeedsUpdate = false;
}

void processUIInput() {
  // Rotary Encoder Processing
  long c = encoder.getCount();
  if (c != lastEncoderCount) {
    long d = lastEncoderCount - c;
    lastEncoderCount = c;
    if (!inSubMenu) {
      menuIndex = (menuIndex + (int)d + MENU_LEN) % MENU_LEN;
      displayNeedsUpdate = true;
    } else if (activeMode == 5) {
      customTarget = constrain(customTarget + 0.5f * d, 0.0f, 120.0f);
      activeTarget = customTarget;
      customTargetDirty = true;
      lastCustomChangeMs = millis();
      displayNeedsUpdate = true;
    }
  }

  // Debounced NVS flush for custom target (protects flash wear)
  if (customTargetDirty && millis() - lastCustomChangeMs >= 2000) {
    prefs.putFloat("customTarget", customTarget);
    customTargetDirty = false;
  }

  // Rotary Encoder Button Click Processing with 50ms Debounce
  static int lastBtn = HIGH;
  int btn = digitalRead(ENCODER_BTN);
  if (lastBtn == HIGH && btn == LOW && (millis() - lastBtnDebounceMs >= BTN_DEBOUNCE_MS)) {
    unsigned long btnNow = millis();
    lastBtnDebounceMs = btnNow;
    if (!inSubMenu && menuIndex != 7) {
      activeMode = menuIndex;
      activeTarget = (activeMode == 5) ? customTarget : menuTargets[activeMode];
      prefs.putInt("activeMode", activeMode);
      if (customTargetDirty) {
        prefs.putFloat("customTarget", customTarget);
        customTargetDirty = false;
      }
      inSubMenu = true;
      waitingForSecondSHOW = false;
      lastBtnPressMs = btnNow;

      if (!isnan(chamberTemp)) {
        if (chamberTemp > activeTarget + HYSTERESIS_TO_FULL) {
          startOpenVent(false);
          updateFan(FAN_HIGH);
        } else if (chamberTemp > activeTarget + HYSTERESIS_TO_HALF) {
          startOpenVent(true);
          updateFan(FAN_LOW);
        } else {
          updateFan(FAN_OFF);
        }
      } else {
        updateFan(FAN_OFF);
      }

      if (activeMode == 6) {
        startAdaptiveCooldown();
      }
      displayNeedsUpdate = true;
    } else {
      if (waitingForSecondSHOW && btnNow - lastBtnPressMs <= DOUBLE_CLICK_MS) {
        inSubMenu = false;
        waitingForSecondSHOW = false;
        if (customTargetDirty) {
          prefs.putFloat("customTarget", customTarget);
          customTargetDirty = false;
        }
        updateFan(FAN_OFF);
        startCloseVent();
        displayNeedsUpdate = true;
      } else {
        waitingForSecondSHOW = true;
        lastBtnPressMs = btnNow;
      }
    }
  }
  lastBtn = btn;
  if (waitingForSecondSHOW && millis() - lastBtnPressMs > DOUBLE_CLICK_MS) {
    waitingForSecondSHOW = false;
  }
}

void renderDisplayIfNeeded() {
  if (displayNeedsUpdate && millis() - lastOledMs >= OLED_INTERVAL_MS) {
    lastOledMs = millis();
    inSubMenu ? drawSubMenu() : drawMainMenu();
  }
}
