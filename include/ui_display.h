/*
 * Chamber Master - UI & Display Module (OLED, Rotary Encoder, Menu System)
 */

#ifndef UI_DISPLAY_H
#define UI_DISPLAY_H

#include "config.h"

void ensureOLED();
void initUI();
void drawChamberTempBlink();
void drawMainMenu();
void drawCooldownScreen();
void drawSubMenu();
void processUIInput();
void renderDisplayIfNeeded();

#endif // UI_DISPLAY_H
