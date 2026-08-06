/*
 * Chamber Master - 3D Printer Enclosure Controller
 * Configuration & System Constants (v2.5 Modular)
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <ESP32Encoder.h>
#include <ESP32Servo.h>
#include <ESP32PWM.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <QRCodeGFX.h>
#include <ESPmDNS.h>

// ==================== WIFI & NETWORK SETTINGS ====================
#ifndef WIFI_SSID
#define WIFI_SSID     "YOUR_WIFI_NAME"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif

#ifndef MDNS_HOSTNAME
#define MDNS_HOSTNAME "enclosure-monitor"
#endif
// =================================================================

// ==================== HARD KILL TRANSISTOR ====================
// Low-side 2N2222 transistor cuts fan GND rail to override CPU fan 20% failsafe
#define FAN_POWER_PIN 15
// ==============================================================

// ==================== PIN ASSIGNMENTS ====================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

#define CHAMBER_PIN   32
#define INTAKE_PIN    13
#define DHT_PIN       23
#define DHTTYPE       DHT11

#define FAN_PIN       33
#define FAN_TACH_PIN  19
#define SERVO_PIN     5

#define ENCODER_CLK   25
#define ENCODER_DT    26
#define ENCODER_BTN   27

#define LED_PIN       2
// ========================================================

// ==================== FAN PWM CONFIGURATION ====================
extern const int FAN_PWM_FREQ;
extern const int FAN_PWM_RES;
extern volatile uint8_t fanDutyCycle;
extern ESP32PWM fanPWM;
extern const uint8_t NORMAL_MIN_DUTY;
// ==============================================================

// ==================== RPM MEASUREMENT ====================
extern volatile unsigned long fanPulseCount;
extern volatile unsigned long lastFanPulseUs;
extern unsigned long lastRpmMs;
extern const unsigned long RPM_SAMPLE_MS;
extern float currentFanRPM;
// =========================================================

// ==================== TEMPERATURE SENSORS ====================
extern OneWire oneWireChamber;
extern DallasTemperature chamberSensor;
extern OneWire oneWireIntake;
extern DallasTemperature intakeSensor;
extern DHT dht;
// ===========================================================

// ==================== ROTARY ENCODER & MENU ====================
extern ESP32Encoder encoder;
extern long lastEncoderCount;

extern const char *menuItems[];
extern const float menuTargets[];
extern const int MENU_LEN;

extern int menuIndex;
extern bool inSubMenu;
extern int activeMode;
extern float customTarget;
extern float activeTarget;
// =============================================================

// ==================== PERSISTENT SETTINGS ====================
extern Preferences prefs;
// ===========================================================

// ==================== DISPLAY UPDATE & SENSOR TIMING ====================
extern unsigned long lastOledMs;
extern const unsigned long OLED_INTERVAL_MS;

extern unsigned long lastTempRequestMs;
extern const unsigned long DS_CONV_MS;
extern bool tempsRequested;

extern unsigned long lastDhtReadMs;
extern const unsigned long DHT_MIN_INTERVAL_MS;

extern float chamberTemp;
extern float intakeTemp;
extern float ambientTemp;
extern float ambientHum;

extern bool displayNeedsUpdate;
// ====================================================================

// ==================== VENT SERVO CONTROL ====================
extern Servo ventServo;
enum VentState { VENT_CLOSED, VENT_HALF_OPENING, VENT_HALF_OPEN, VENT_OPENING, VENT_OPEN, VENT_CLOSING };
extern VentState ventState;
extern unsigned long ventActionStartMs;

extern const unsigned long SERVO_OPEN_TIME;
extern const unsigned long SERVO_HALF_OPEN_TIME;
extern const unsigned long SERVO_CLOSE_TIME;
extern const unsigned long SERVO_FULL_TO_HALF_TIME;

extern const int SERVO_STOP_PULSE;
extern const int SERVO_FORWARD_PULSE;  // Close vent
extern const int SERVO_REVERSE_PULSE;  // Open vent
// ===========================================================

// ==================== FAN STATE CONTROL ====================
enum FanSpeed { FAN_OFF, FAN_LOW, FAN_HIGH };
extern FanSpeed fanSpeed;
extern bool ledOn;

extern const float HYSTERESIS_TO_HALF;
extern const float HYSTERESIS_TO_CLOSED;
extern const float HYSTERESIS_TO_FULL;
extern const float HYSTERESIS_FROM_FULL;
// ===========================================================

// ==================== UI BUTTON & DEBOUNCE ====================
extern bool chamberTempVisible;
extern unsigned long lastBlinkMs;
extern const unsigned long BLINK_INTERVAL_MS;

extern unsigned long lastBtnPressMs;
extern const unsigned long DOUBLE_CLICK_MS;
extern bool waitingForSecondSHOW;
extern unsigned long lastBtnDebounceMs;
extern const unsigned long BTN_DEBOUNCE_MS;

extern unsigned long lastCustomChangeMs;
extern bool customTargetDirty;
// ============================================================

// ==================== WIFI & WEB SERVER ====================
extern const char *ssid;
extern const char *password;
extern WebServer server;
// ==========================================================

// ==================== COOLDOWN LOGIC ====================
extern float cooldownStartTemp;
extern float cooldownLastTemp;
extern unsigned long cooldownLastCheckMs;
extern uint8_t cooldownFanDuty;

extern const float COOLDOWN_RATE_DEG_PER_MIN;
extern const float COOLDOWN_TARGET_OFFSET;
extern const unsigned long COOLDOWN_SAMPLE_MS;

extern unsigned long cooldownStartMs;
extern long cooldownEstSeconds;
extern float cooldownProgress;
extern unsigned long lastCountdownSecMs;
// =====================================================

// ==================== SAFETY & DISPLAY OBJECTS ====================
extern bool intakeFault;
extern Adafruit_SSD1306 display;
extern QRCodeGFX qrcode;
// ================================================================

// ==================== STARTUP VENT CALIBRATION ====================
enum StartupVentState { VENT_START_CLOSING, VENT_START_WAIT_CLOSED, VENT_START_OPENING, VENT_START_WAIT_OPEN, VENT_START_CLOSING_AGAIN, VENT_START_WAIT_CLOSED_AGAIN, VENT_START_DONE };
extern StartupVentState startupVentState;
extern unsigned long startupVentTimer;
extern bool startupVentDone;
// ================================================================

#endif // CONFIG_H
