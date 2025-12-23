/*
 * 3D Printer Enclosure Controller
 * 
 * Description:
 * This is a complete controller for a 3D printer enclosure.
 * It monitors temperatures, controls an exhaust fan and vent servo,
 * provides safe adaptive cooldown for materials like ABS/ASA,
 * and includes safety features like intake fault detection.
 * 
 * Key Features:
 * - Chamber, Intake, and Ambient temperature monitoring
 * - Adaptive cooldown mode (starts at 20% fan, never below 20% while running)
 * - Hard kill transistor for true 0 RPM when fan duty < 20%
 * - Intake fault detection with emergency max cooling
 * - Rotary encoder menu navigation with SSD1306 OLED display
 * - Vent servo control with smart directional hysteresis (new stable logic)
 * - Web dashboard with live stats, cooldown progress, and fault alert banner
 * - mDNS support (access at http://enclosure-monitor.local)
 * - Persistent storage for active mode and custom target
 * 
 * Hardware Requirements:
 * - ESP32 development board
 * - SSD1306 128x64 OLED display
 * - 2× DS18B20 temperature sensors (chamber and intake)
 * - DHT11 for ambient temperature and humidity
 * - Rotary encoder with push button
 * - SG90 or similar servo for vent control
 * - 4-pin PC fan (PWM + tachometer)
 * - 2N2222 transistor + 1kΩ resistor for low-side hard kill on fan GND
 * Date: December 19, 2025
 * License: MIT License (feel free to modify and share)
 */

#define DEBUG // Comment this line to disable Serial debug output

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
#include <nvs_flash.h>
#include <QRCodeGFX.h>
#include <ESPmDNS.h>

// ==================== HARD KILL FOR TRUE 0 RPM ====================
// GPIO 15 controls the base of a 2N2222 transistor (via 1kΩ resistor)
// This is a low-side switch on the fan's ground line for true 0 RPM
#define FAN_POWER_PIN 15
// ==================================================================

// ==================== PIN ASSIGNMENTS ====================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1  // Reset pin not used (-1)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define CHAMBER_PIN   32  // DS18B20 inside the enclosure
#define INTAKE_PIN    13  // DS18B20 on fresh air intake path
#define DHT_PIN       23  // DHT11 for ambient temperature and humidity
#define DHTTYPE       DHT11

#define FAN_PIN       33  // PWM signal to fan's blue control wire
#define FAN_TACH_PIN  19  // Tachometer signal from fan (yellow wire)
#define SERVO_PIN     5   // Signal pin for vent servo

#define ENCODER_CLK   25
#define ENCODER_DT    26
#define ENCODER_BTN   27

#define LED_PIN       2   // Built-in LED (indicates active fan/vent)
// ========================================================

// ==================== FAN PWM CONFIGURATION ====================
// 1000 Hz provides a good balance: minimal audible whine and reliable control
// with the low-side transistor hard kill setup
const int FAN_PWM_FREQ = 1000;
const int FAN_PWM_RES  = 8;
volatile uint8_t fanDutyCycle = 0;
ESP32PWM fanPWM;

// Minimum fan duty during cooldown (20% ≈ 51/255) to avoid thermal shock
const uint8_t COOLDOWN_MIN_DUTY = 51;
// ==============================================================

// ==================== RPM MEASUREMENT ====================
volatile unsigned long fanPulseCount = 0;  // Counts tach pulses
unsigned long lastRpmMs = 0;
const unsigned long RPM_SAMPLE_MS = 1000;  // Sample every second
float currentFanRPM = 0.0;
// =========================================================

// ==================== TEMPERATURE SENSORS ====================
OneWire oneWireChamber(CHAMBER_PIN);
DallasTemperature chamberSensor(&oneWireChamber);
OneWire oneWireIntake(INTAKE_PIN);
DallasTemperature intakeSensor(&oneWireIntake);
DHT dht(DHT_PIN, DHTTYPE);
// ===========================================================

// ==================== ROTARY ENCODER ====================
ESP32Encoder encoder;
long lastEncoderCount = 0;
// ========================================================

// ==================== MENU SYSTEM ====================
const char *menuItems[] = {"PLA", "ASA", "ABS", "TPU", "PETG", "CUSTOM", "COOLDOWN", "QR CODE"};
const float menuTargets[] = {30.0, 50.0, 60.0, 25.0, 40.0, -1.0, -2.0, -3.0};
const int MENU_LEN = 8;

int menuIndex = 0;
bool inSubMenu = false;
int activeMode = 0;
float customTarget = 30.0;
float activeTarget = 30.0;
// ====================================================

// ==================== PERSISTENT SETTINGS ====================
Preferences prefs;  // Uses NVS for non-volatile storage
// ===========================================================

// ==================== DISPLAY UPDATE ====================
unsigned long lastOledMs = 0;
const unsigned long OLED_INTERVAL_MS = 200;  // Refresh rate

unsigned long lastTempRequestMs = 0;
const unsigned long DS_CONV_MS = 750;        // DS18B20 conversion time
bool tempsRequested = false;

unsigned long lastDhtReadMs = 0;
const unsigned long DHT_MIN_INTERVAL_MS = 3500;

float chamberTemp = NAN;
float intakeTemp = NAN;
float ambientTemp = NAN;
float ambientHum = NAN;

bool displayNeedsUpdate = true;
// ====================================================

// ==================== VENT SERVO CONTROL ====================
// These timing values are tuned for a standard SG90 servo.
// You can adjust them to match your specific servo or vent mechanism.
// Longer times = slower movement, shorter = faster.
Servo ventServo;
enum VentState {VENT_CLOSED, VENT_HALF_OPENING, VENT_HALF_OPEN, VENT_OPENING, VENT_OPEN, VENT_CLOSING};
VentState ventState = VENT_CLOSED;
unsigned long ventActionStartMs = 0;

const unsigned long SERVO_OPEN_TIME = 850;          // Time to go from closed to full open (ms)
const unsigned long SERVO_HALF_OPEN_TIME = 425;     // Time for half open from closed
const unsigned long SERVO_CLOSE_TIME = 1100;        // Time to fully close from open
const unsigned long SERVO_FULL_TO_HALF_TIME = 750;  // Time from full open to half open

const int SERVO_STOP_PULSE = 1500;     // Stop
const int SERVO_FORWARD_PULSE = 2000;  // Direction to close vent
const int SERVO_REVERSE_PULSE = 1000;  // Direction to open vent
// ===========================================================

// ==================== FAN STATE CONTROL ====================
enum FanSpeed {FAN_OFF, FAN_LOW, FAN_HIGH};
FanSpeed fanSpeed = FAN_OFF;
bool ledOn = false;

// Hysteresis thresholds for smart directional control (all normal modes)
const float HYSTERESIS_TO_HALF = -1.0f;   // From closed: activate if > target + HYSTERESIS_TO_HALF
const float HYSTERESIS_TO_CLOSED = -2.0f; // From half: close if < target + HYSTERESIS_TO_CLOSED
const float HYSTERESIS_TO_FULL = 2.0f;    // From half: full open if > target + HYSTERESIS_TO_FULL
const float HYSTERESIS_FROM_FULL = 1.0f;  // From full: drop to half if <= target + HYSTERESIS_FROM_FULL
// ===========================================================

// ==================== UI ELEMENTS ====================
bool chamberTempVisible = true;
unsigned long lastBlinkMs = 0;
const unsigned long BLINK_INTERVAL_MS = 500;

unsigned long lastBtnPressMs = 0;
const unsigned long DOUBLE_CLICK_MS = 250;
bool waitingForSecondSHOW = false;
// ====================================================

// ==================== WIFI SETTINGS ====================
// Replace the asterisks with your actual WiFi credentials
const char *ssid = "*************";
const char *password = "***********";
WebServer server(80);
// ======================================================

// ==================== COOLDOWN LOGIC ====================
float cooldownLastTemp = 0.0;
unsigned long cooldownLastCheckMs = 0;
uint8_t cooldownFanDuty = COOLDOWN_MIN_DUTY;

const float COOLDOWN_RATE_DEG_PER_MIN = 1.5;     // Desired cooling rate
const float COOLDOWN_TARGET_OFFSET = 3.0;        // Final target = ambient + 3°C
const unsigned long COOLDOWN_SAMPLE_MS = 60000;  // Adjust fan every 60 seconds

unsigned long cooldownStartMs = 0;
long cooldownEstSeconds = 0;
float cooldownProgress = 0.0;
// =====================================================

// ==================== SAFETY FEATURES ====================
bool intakeFault = false;  // Emergency mode if intake air is hotter than chamber
// ========================================================

// ==================== QR CODE DISPLAY ====================
QRCodeGFX qrcode(display);
// ========================================================

// ==================== STARTUP VENT CALIBRATION ====================
enum StartupVentState {VENT_START_CLOSING, VENT_START_WAIT_CLOSED, VENT_START_OPENING, VENT_START_WAIT_OPEN, VENT_START_CLOSING_AGAIN, VENT_START_WAIT_CLOSED_AGAIN, VENT_START_DONE};
StartupVentState startupVentState = VENT_START_CLOSING;
unsigned long startupVentTimer = 0;
bool startupVentDone = false;
// ================================================================

// ==================== FAN TACH INTERRUPT ====================
void IRAM_ATTR fanPulseISR() {
  fanPulseCount++;  // Increment on each falling edge (2 pulses per revolution)
}
// ===========================================================

// ==================== OLED INITIALIZATION ====================
void ensureOLED() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    #ifdef DEBUG
    Serial.println(F("SSD1306 allocation failed"));
    #endif
    for (;;) delay(1000);  // Halt if display fails
  }
  display.clearDisplay();
  display.display();
}
// ===========================================================

// ==================== BLINKING CHAMBER TEMP ====================
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
// ===========================================================

// ==================== MAIN MENU DISPLAY ====================
void drawMainMenu() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  int16_t x1, y1;
  uint16_t w, h;

  if (menuIndex == 6) {  // COOLDOWN
    display.setTextSize(2);
    display.getTextBounds("COOLDOWN", 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2);
    display.print("COOLDOWN");
  } else if (menuIndex == 7) {  // QR CODE
    if (WiFi.status() == WL_CONNECTED) {
      String url = "http://enclosure-monitor.local";
      qrcode.setScale(2);
      qrcode.draw(url, 46, 8);
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
  } else {  // Material modes
    display.setTextSize(3);
    display.getTextBounds(menuItems[menuIndex], 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2);
    display.print(menuItems[menuIndex]);
    display.setTextSize(1);
    display.setCursor((SCREEN_WIDTH - 80) / 2, (SCREEN_HEIGHT - h) / 2 + h + 6);
    display.printf("Target: %.1fC", menuTargets[menuIndex] == -1.0 ? customTarget : menuTargets[menuIndex]);
  }
  drawChamberTempBlink();
  display.display();
  displayNeedsUpdate = false;
}
// ===========================================================

// ==================== COOLDOWN SCREEN ====================
void drawCooldownScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 0);
  display.print("COOLING DOWN");
  display.setCursor(0, 12);
  display.printf("Fan: %3d%% (RPM: %4d)", (fanDutyCycle * 100) / 255, (int)currentFanRPM);

  int mins = cooldownEstSeconds / 60, secs = cooldownEstSeconds % 60;
  display.setCursor(0, 24);
  if (cooldownEstSeconds <= 0) display.print("Time: Cooled");
  else if (cooldownEstSeconds >= 3600) display.printf("Time: %dh", cooldownEstSeconds / 3600);
  else if (cooldownEstSeconds >= 120) display.printf("Time: %dm", mins);
  else display.printf("Time: %dm%02ds", mins, secs);

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
// ========================================================

// ==================== SUBMENU DISPLAY ====================
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
// ========================================================

// ==================== TEMPERATURE READING ====================
void requestTempsNonBlocking() {
  chamberSensor.requestTemperatures();
  intakeSensor.requestTemperatures();
  tempsRequested = true;
  lastTempRequestMs = millis();
}

void readTempsAfterDelay() {
  float c = chamberSensor.getTempCByIndex(0);
  float i = intakeSensor.getTempCByIndex(0);
  chamberTemp = (c == -127.0 || c == 85.0) ? NAN : c;
  intakeTemp = (i == -127.0 || i == 85.0) ? NAN : i;
  tempsRequested = false;
  displayNeedsUpdate = true;
}
// ===========================================================

// ==================== VENT CONTROL FUNCTIONS ====================
void startOpenVent(bool half = false) {
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
// =============================================================

// ==================== LED INDICATOR ====================
void updateLED() {
  bool on = (fanSpeed != FAN_OFF) && (ventState == VENT_OPEN || ventState == VENT_HALF_OPEN || ventState == VENT_OPENING || ventState == VENT_HALF_OPENING);
  if (on != ledOn) {
    ledOn = on;
    digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
  }
}
// ======================================================

// ==================== FAN CONTROL WITH HARD KILL ====================
void setFanDuty(uint8_t d) {
  fanDutyCycle = d;

  // Hard kill: cut power if duty = 0
  digitalWrite(FAN_POWER_PIN, (d > 0) ? HIGH : LOW);

  fanPWM.writeScaled(d / 255.0f);
  displayNeedsUpdate = true;
}

void updateFan(FanSpeed s) {
  if (s != fanSpeed) {
    fanSpeed = s;
    setFanDuty(s == FAN_HIGH ? 255 : s == FAN_LOW ? 140 : 0);
  }
}
// =================================================================

// ==================== COOLDOWN WEB HANDLER ====================
void handleStartCooldown() {
  if (server.method() == HTTP_POST || server.method() == HTTP_GET) {
    activeMode = 6;
    inSubMenu = true;
    activeTarget = -2.0;
    prefs.putInt("activeMode", activeMode);
    cooldownStartMs = cooldownLastCheckMs = millis();
    cooldownLastTemp = chamberTemp;
    cooldownFanDuty = COOLDOWN_MIN_DUTY;
    setFanDuty(COOLDOWN_MIN_DUTY);
    startOpenVent(false);
    displayNeedsUpdate = true;
    server.send(200, "application/json", "{\"status\":\"COOLDOWN_STARTED\"}");
  } else {
    server.send(405, "text/plain", "Method Not Allowed");
  }
}
// ===========================================================

// ==================== WEB DASHBOARD (with fault banner) ====================
void handleRoot() {
  String html = R"=====(<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>ENCLOSURE DASH</title>
<style>:root{--bg:#121212;--card:#1e1e24;--glass:rgba(255,255,255,0.03);--accent:#9c27b0;--accent-glow:#9c27b033;--text:#f8fafc;--muted:#94a3b8;--danger:#ef4444}*{margin:0;padding:0;box-sizing:border-box}body{background:var(--bg);color:var(--text);min-height:100vh;padding:1rem;background-image:radial-gradient(circle at 20% 80%,rgba(156,39,176,.1)0%,transparent 50%),radial-gradient(circle at 80% 20%,rgba(233,30,99,.1)0%,transparent 50%);line-height:1.5}
.container{max-width:1200px;margin:auto}h1{text-align:center;font-size:clamp(1.8rem,5vw,2.5rem);margin-bottom:1.5rem;background:linear-gradient(90deg,#9c27b0,#e91e63);-webkit-background-clip:text;background-clip:text;color:transparent;font-weight:800;letter-spacing:1px;text-shadow:0 0 20px var(--accent-glow)}
.card{background:var(--card);backdrop-filter:blur(12px);border-radius:1.5rem;padding:1.5rem;margin-bottom:1.5rem;border:1px solid rgba(255,255,255,.1);box-shadow:0 8px 32px rgba(0,0,0,.3),0 0 20px var(--accent-glow);transition:.3s;position:relative;overflow:hidden}
.card:hover{transform:translateY(-4px);box-shadow:0 12px 40px rgba(0,0,0,.4),0 0 30px var(--accent-glow)}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:1rem}
.stat{text-align:center;padding:.8rem;background:var(--glass);border-radius:1rem;transition:.3s;position:relative}
.stat:hover{background:rgba(156,39,176,.1);transform:scale(1.05)}
.label{font-size:.8rem;color:var(--muted);text-transform:uppercase;letter-spacing:1px;display:flex;align-items:center;justify-content:center;gap:6px;margin-bottom:.3rem}
.label svg{width:36px;height:36px;fill:var(--accent)}
.value{font-size:1.4rem;font-weight:700;color:var(--accent);text-shadow:0 0 10px var(--accent-glow)}
.fan-wrapper{width:60px;height:60px;margin:0 auto .5rem;position:relative}
.fan-core{position:absolute;top:50%;left:50%;width:10px;height:10px;background:var(--accent);border-radius:50%;transform:translate(-50%,-50%);box-shadow:0 0 10px var(--accent-glow);z-index:5}
.fan-blade{position:absolute;width:10px;height:28px;background:var(--accent);border-radius:4px;top:50%;left:50%;transform-origin:50% 0%;box-shadow:0 0 10px var(--accent-glow)}
.fan-blade:nth-child(1){transform:translate(-50%,0%) rotate(0deg)}.fan-blade:nth-child(2){transform:translate(-50%,0%) rotate(72deg)}
.fan-blade:nth-child(3){transform:translate(-50%,0%) rotate(144deg)}.fan-blade:nth-child(4){transform:translate(-50%,0%) rotate(216deg)}
.fan-blade:nth-child(5){transform:translate(-50%,0%) rotate(288deg)}@keyframes spin{to{transform:rotate(360deg)}}
.spinning{animation:spin linear infinite}
#cooldownCard{background:linear-gradient(135deg,rgba(255,59,92,.2),rgba(156,39,176,.1));border:1px solid var(--danger)}
.progress-ring{width:100px;height:100px;margin:1rem auto}
.progress-ring circle{cx:50;cy:50;r:40;fill:none;stroke-width:8;stroke-linecap:round}
.progress-ring .bg{stroke:rgba(255,255,255,.1)}.progress-ring .fg{stroke:url(#gradient);transform:rotate(-90deg);transform-origin:50% 50%}
.time{text-align:center;font-size:1.2rem;font-weight:700;color:var(--accent)}
.cam-wrapper{position:relative;width:100%;padding-top:56.25%;border-radius:1rem;overflow:hidden;background:#000;box-shadow:0 0 20px rgba(0,0,0,.5)}
#camFrame{position:absolute;top:0;left:0;width:100%;height:100%;border:none}
.btn{background:linear-gradient(90deg,#9c27b0,#e91e63);color:#fff;border:none;border-radius:1rem;padding:1rem 2rem;font-size:1.1rem;font-weight:700;cursor:pointer;width:100%;margin-top:1rem;box-shadow:0 0 25px #9c27b033,inset 0 0 10px #9c27b044;transition:.3s;text-transform:uppercase;letter-spacing:1px}
.btn:hover{background:linear-gradient(90deg,#e91e63,#9c27b0);box-shadow:0 0 40px #9c27b066,0 0 10px #e91e6366;transform:translateY(-2px)}
.btn:active{transform:scale(.96)}
#faultBanner{position:fixed;top:10px;left:50%;transform:translateX(-50%);background:#ff0000;color:white;padding:15px 30px;border-radius:10px;font-weight:bold;font-size:1.2rem;z-index:1000;box-shadow:0 0 20px #ff0000aa;text-transform:uppercase;letter-spacing:1px;animation:blink 1s infinite alternate;display:none;}
@keyframes blink{from{opacity:0.8} to{opacity:1}}
</style>
</head><body>
<div class="container">
<h1>ENCLOSURE DASH</h1>
<div id="faultBanner">INTAKE FAULT! EMERGENCY COOLING ACTIVE</div>
<div class="card"><div class="grid">
<!-- CHAMBER = BOX -->
<div class="stat"><div class="label"><svg viewBox="0 0 24 24"><path d="M20 7h-4V5l-2-2h-4L8 5v2H4c-1.1 0-2 .9-2 2v10c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V9c0-1.1-.9-2-2-2zm-8-2h4v2h-4V5zm8 14H4V9h16v10z"/></svg>Chamber</div><div class="value" id="chamber">--.-°C</div></div>

<!-- INTAKE -->
<div class="stat"><div class="label"><svg viewBox="0 0 24 24"><path d="M12 2c5.52 0 10 4.48 10 10s-4.48 10-10 10S2 17.52 2 12 6.48 2 12 2zm0 18c4.42 0 8-3.58 8-8s-3.58-8-8-8-8 3.58-8 8 3.58 8 8 8zm1-5h-2v-2h2v2zm0-4h-2V7h2v4z"/></svg>Intake</div><div class="value" id="intake">--.-°C</div></div>

<!-- AMBIENT = THERMOMETER -->
<div class="stat"><div class="label"><svg viewBox="0 0 24 24"><path d="M15 13V5c0-1.66-1.34-3-3-3S9 3.34 9 5v8c-1.21.91-2 2.37-2 4 0 2.76 2.24 5 5 5s5-2.24 5-5c0-1.63-.79-3.09-2-4zm-3-8c.55 0 1 .45 1 1v4h-2V6c0-.55.45-1 1-1z"/></svg>Ambient</div><div class="value" id="ambient">--.-°C</div></div>

<!-- HUMIDITY = DROPLET -->
<div class="stat"><div class="label"><svg viewBox="0 0 24 24"><path d="M11.54 2c-3.08 0-5.72 1.88-6.85 4.55-.32.75-.49 1.56-.49 2.4 0 2.76 2.24 5 5 5h.01c.28 0 .54-.02.8-.06l.19-.04c.22-.05.43-.12.63-.22.27-.14.52-.32.75-.54.3-.3.56-.64.77-1.02.18-.33.32-.69.42-1.06.08-.29.14-.59.17-.89.03-.28.04-.56.04-.84 0-1.97-1.6-3.57-3.57-3.57z"/></svg>Humidity</div><div class="value" id="humidity">--%</div></div>

<!-- MODE = GEAR -->
<div class="stat"><div class="label"><svg viewBox="0 0 24 24"><path d="M19.43 12.98c.04-.32.07-.64.07-.98 0-.34-.03-.66-.07-.98l2.11-1.65c.19-.15.24-.42.12-.64l-2-3.46c-.12-.22-.39-.3-.61-.22l-2.49 1c-.52-.4-1.08-.73-1.69-.98l-.38-2.65C14.46 2.18 14.25 2 14 2h-4c-.25 0-.46.18-.49.42l-.38 2.65c-.61.25-1.17.59-1.69.98l-2.49-1c-.23-.09-.49 0-.61.22l-2 3.46c-.13.22-.07.49.12.64l2.11 1.65c-.04.32-.07.65-.09.98 0 .33.03.66.07.98l-2.11 1.65c-.19.15-.24.42-.12.64l2 3.46c.12.22.39.3.61.22l2.49-1c.52.4 1.08.73 1.69.98l.38 2.65c.03.24.24.42.49.42h4c.25 0 .46-.18.49-.42l.38-2.65c.61-.25 1.17-.59 1.69-.98l2.49 1c.23.09.49 0 .61-.22l2-3.46c.12-.22.07-.49-.12-.64l-2.11-1.65zM12 15.5c-1.93 0-3.5-1.57-3.5-3.5s1.57-3.5 3.5-3.5 3.5 1.57 3.5 3.5-1.57 3.5-3.5 3.5z"/></svg>Mode</div><div class="value" id="mode">---</div></div>

<!-- TARGET = CROSSHAIR -->
<div class="stat"><div class="label"><svg viewBox="0 0 24 24"><path d="M12 8c-2.21 0-4 1.79-4 4s1.79 4 4 4 4-1.79 4-4-1.79-4-4-4zm8.94 3c-.46-4.17-3.77-7.48-7.94-7.94V2h-2v1.06C6.83 3.52 3.52 6.83 3.06 11H2v2h1.06c.46 4.17 3.77 7.48 7.94 7.94V22h2v-1.06c4.17-.46 7.48-3.77 7.94-7.94H22v-2h-1.06zM12 19c-3.87 0-7-3.13-7-7s3.13-7 7-7 7 3.13 7 7-3.13 7-7 7z"/></svg>Target</div><div class="value" id="target">---</div></div>

<!-- FAN -->
<div class="stat" id="fanStat">
  <div class="fan-wrapper" id="fanIcon">
    <div class="fan-blade"></div><div class="fan-blade"></div><div class="fan-blade"></div><div class="fan-blade"></div><div class="fan-blade"></div>
    <div class="fan-core"></div>
  </div>
  <div class="label"><svg viewBox="0 0 24 24"><path d="M12 12c0-3.31 2.69-6 6-6 1.01 0 1.96.25 2.8.7l-1.46 1.46C18.32 8.06 17.18 8 16 8c-2.21 0-4 1.79-4 4s1.79 4 4 4c1.18 0 2.32-.06 3.34-.16l1.46 1.46c-.84.45-1.79.7-2.8.7-3.31 0-6-2.69-6-6zm-6 0c0 3.31-2.69 6-6 6-.88 0-1.71-.18-2.48-.5l1.46-1.46C5.68 16.94 6.82 17 8 17c2.21 0 4-1.79 4-4s-1.79-4-4-4c-1.18 0-2.32-.06-3.34.16l-1.46-1.46C4.04 7.25 4.99 7 6 7c3.31 0 6 2.69 6 6z"/></svg>Fan</div>
  <div class="value" id="fanSpeed">0%</div>
</div>

<!-- VENT -->
<div class="stat"><div class="label"><svg viewBox="0 0 24 24"><path d="M3 13h2v-2H3v2zm4 8h2v-2H7v2zm8-12h-2V7h2v2zm-4 4h2v-2h-2v2zm8 4h-2v-2h2v2zM3 9h2V7H3v2zm8 8h2v-2h-2v2zm-4-4H9v-2h2v2zm8-8h-2V5h2v2z"/></svg>Vent</div><div class="value" id="vent">CLOSED</div></div>

</div></div>
<div class="card" id="cooldownCard" style="display:none"><h2 style="text-align:center;margin-bottom:1rem;color:var(--danger);text-shadow:0 0 10px #ff3b5c66">COOLING DOWN</h2>
<div class="progress-ring"><svg width="100" height="100"><defs><linearGradient id="gradient"><stop offset="0%" stop-color="#ff3b5c"/><stop offset="100%" stop-color="#9c27b0"/></linearGradient></defs><circle class="bg" r="40" cx="50" cy="50"/><circle class="fg" r="40" cx="50" cy="50" stroke-dasharray="251.3" stroke-dashoffset="251.3"/></svg></div>
<div class="time" id="timeLeft">Calculating...</div>
<div style="display:grid;grid-template-columns:1fr 1fr;gap:1rem;margin-top:1rem"><div class="stat"><div class="label">Fan</div><div class="value" id="fanInfo">---% (---- RPM)</div></div><div class="stat"><div class="label">Ambient</div><div class="value" id="ambInfo">--.-°C --%</div></div></div></div>
<div class="card"><h3 style="margin-bottom:.8rem;color:var(--accent);display:flex;align-items:center;gap:8px">LIVE CAMERA</h3><div class="cam-wrapper" id="camWrapper"><iframe id="camFrame" src="http://3d-print-live.local/" title="Live Feed"></iframe></div></div>
<button class="btn" id="cooldownBtn">START COOLDOWN</button>
</div>
<script>
const camBase="http://3d-print-live.local";
async function update(){
  try{
    const r=await fetch('/status'),d=await r.json();
    
    document.getElementById('chamber').textContent = d.chamberTemp?.toFixed(1) + '°C' || '--.-°C';
    document.getElementById('intake').textContent = d.intakeTemp?.toFixed(1) + '°C' || '--.-°C';
    document.getElementById('ambient').textContent = d.ambientTemp?.toFixed(1) + '°C' || '--.-°C';
    document.getElementById('humidity').textContent = d.ambientHum + '%' || '--%';
    document.getElementById('mode').textContent = d.activeMode;
    document.getElementById('target').textContent = d.targetTemp;
    document.getElementById('fanSpeed').textContent = d.fanDuty + '%';
    document.getElementById('vent').textContent = d.ventState;

    const newFan = d.fanDuty;
    const fanIcon = document.getElementById('fanIcon');
    if (newFan > 5) {
      const speed = (100 - newFan) * 0.02 + 0.2;
      fanIcon.style.animationDuration = speed + 's';
      fanIcon.classList.add('spinning');
    } else {
      fanIcon.classList.remove('spinning');
    }

    // Intake fault banner
    const faultBanner = document.getElementById('faultBanner');
    if (d.fault && d.fault !== "NONE") {
      faultBanner.style.display = 'block';
    } else {
      faultBanner.style.display = 'none';
    }

    const cooling = d.activeMode === 'COOLDOWN';
    document.getElementById('cooldownCard').style.display = cooling ? 'block' : 'none';
    document.getElementById('cooldownBtn').style.display = cooling ? 'none' : 'block';

    if (cooling) {
      const prog = Math.min(d.progress ?? 0, 1);
      const circ = 251.3;
      document.querySelector('.fg').style.strokeDashoffset = circ * (1 - prog);

      document.getElementById('fanInfo').textContent = `${d.fanDuty}% (${d.fanRpm} RPM)`;
      document.getElementById('ambInfo').textContent = `${d.ambientTemp?.toFixed(1)}°C ${d.ambientHum}%`;

      const s = d.estSeconds || 0;

      let timeText;
      if (s === 0) {
        timeText = 'Cooled';
      } else if (s < 0) {
        timeText = 'Calculating...';
      } else if (s >= 3600) {
        timeText = `${Math.floor(s/3600)}h`;
      } else if (s >= 120) {
        timeText = `${Math.floor(s/60)}m`;
      } else {
        timeText = `${Math.floor(s/60)}m${String(s%60).padStart(2,'0')}s`;
      }
      document.getElementById('timeLeft').textContent = timeText;
    }

  } catch(e) {
    console.error(e);
  }
}

document.getElementById('cooldownBtn').onclick = async() => {
  await fetch('/start_cooldown', {method:'POST'});
  update();
};

const camWrapper = document.getElementById('camWrapper');
async function initCam(){
  try{
    const r = await fetch(`${camBase}/status`);
    if(!r.ok) throw 0;
    const d = await r.json();
    const w = d.framesize_width || 1280, h = d.framesize_height || 1024;
    camWrapper.style.paddingTop = (h/w*100) + '%';
  } catch {
    camWrapper.style.paddingTop = '56.25%';
  }
  fetch(`${camBase}/control?var=framesize&val=9`);
  fetch(`${camBase}/control?var=quality&val=5`);
}

window.addEventListener('load', () => {
  update();
  initCam();
  setInterval(update, 1000);
});
</script>
</body></html>)=====";
  server.send(200, "text/html", html);
}

// STATUS JSON
void handleStatus() {
  String r = "{";
  r += "\"chamberTemp\":"; r += isnan(chamberTemp) ? "null" : String(chamberTemp, 1);
  r += ",\"intakeTemp\":"; r += isnan(intakeTemp) ? "null" : String(intakeTemp, 1);
  r += ",\"ambientTemp\":"; r += isnan(ambientTemp) ? "null" : String(ambientTemp, 1);
  r += ",\"ambientHum\":"; r += isnan(ambientHum) ? "null" : String((int)ambientHum);
  r += ",\"fanRpm\":"; r += (int)currentFanRPM;
  r += ",\"fanDuty\":"; r += (fanDutyCycle * 100) / 255;
  r += ",\"activeMode\":\""; r += (activeMode >= 0 && activeMode < MENU_LEN) ? menuItems[activeMode] : "UNKNOWN";
  r += "\",\"targetTemp\":\""; r += (activeMode == 6) ? "COOLDOWN" : String(activeMode == 5 ? customTarget : menuTargets[activeMode], 1);
  r += "\",\"ventState\":\""; switch(ventState){case VENT_CLOSED:r+="CLOSED";break;case VENT_HALF_OPEN:r+="HALF OPEN";break;case VENT_OPEN:r+="OPEN";break;default:r+="MOVING";}
  r += "\",\"fault\":\""; r += intakeFault ? "INTAKE HIGH" : "NONE";
  r += "\",\"estSeconds\":"; r += cooldownEstSeconds;
  r += ",\"progress\":"; r += String(cooldownProgress, 3);
  r += "}";
  server.send(200, "application/json", r);
}

// WiFi & SERVER
void setupWiFiAndServer() {
  WiFi.setHostname("ENCLOSURE MONITOR");
  WiFi.begin(ssid, password);
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 20000) delay(500);
  if (WiFi.status() == WL_CONNECTED) {
    MDNS.begin("enclosure-monitor");
    MDNS.addService("http", "tcp", 80);
    server.on("/", handleRoot);
    server.on("/status", handleStatus);
    server.on("/start_cooldown", HTTP_POST, handleStartCooldown);
    server.on("/start_cooldown", HTTP_GET, handleStartCooldown);
    server.begin();
  }
}

// SETUP
void setup() {
  #ifdef DEBUG
  Serial.begin(115200);
  delay(50);
  Serial.println("Starting...");
  #endif
  ensureOLED();
  prefs.begin("chamber_prefs", false);
  activeMode = prefs.getInt("activeMode", 0);
  if (activeMode < 0 || activeMode >= MENU_LEN) activeMode = 0;
  customTarget = prefs.getFloat("customTarget", 30.0);
  if (customTarget < 0 || customTarget > 120) customTarget = 30.0;
  prefs.putFloat("customTarget", customTarget);
  activeTarget = (activeMode == 5) ? customTarget : menuTargets[activeMode];
  menuIndex = activeMode;
  inSubMenu = true;

  chamberSensor.begin();
  intakeSensor.begin();
  chamberSensor.setResolution(10);
  intakeSensor.setResolution(10);
  chamberSensor.setWaitForConversion(false);
  intakeSensor.setWaitForConversion(false);
  dht.begin();

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(ENCODER_BTN, INPUT_PULLUP);
  encoder.attachHalfQuad(ENCODER_CLK, ENCODER_DT);
  encoder.setCount(0);
  lastEncoderCount = encoder.getCount();

  pinMode(FAN_POWER_PIN, OUTPUT);
  digitalWrite(FAN_POWER_PIN, LOW);

  pinMode(FAN_PIN, OUTPUT);
  fanPWM.attachPin(FAN_PIN, FAN_PWM_FREQ, FAN_PWM_RES);
  fanPWM.writeScaled(0.0f);
  pinMode(FAN_TACH_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FAN_TACH_PIN), fanPulseISR, FALLING);

  ventServo.attach(SERVO_PIN, 500, 2500);
  ventServo.setPeriodHertz(50);
  ventServo.writeMicroseconds(SERVO_STOP_PULSE);
  ventState = VENT_CLOSED;
  startupVentState = VENT_START_CLOSING;
  ventServo.writeMicroseconds(SERVO_FORWARD_PULSE);
  startupVentTimer = millis();

  setupWiFiAndServer();
  requestTempsNonBlocking();
  lastDhtReadMs = millis() - DHT_MIN_INTERVAL_MS;
  lastRpmMs = millis();
  drawSubMenu();
}

// LOOP
void loop() {
  if (!startupVentDone) {
    unsigned long now = millis();
    switch (startupVentState) {
      case VENT_START_CLOSING: if (now - startupVentTimer >= SERVO_CLOSE_TIME) { ventServo.writeMicroseconds(SERVO_STOP_PULSE); startupVentState = VENT_START_WAIT_CLOSED; startupVentTimer = now; } break;
      case VENT_START_WAIT_CLOSED: if (now - startupVentTimer >= 1000) { ventServo.writeMicroseconds(SERVO_REVERSE_PULSE); startupVentState = VENT_START_OPENING; startupVentTimer = now; } break;
      case VENT_START_OPENING: if (now - startupVentTimer >= SERVO_OPEN_TIME) { ventServo.writeMicroseconds(SERVO_STOP_PULSE); startupVentState = VENT_START_WAIT_OPEN; startupVentTimer = now; } break;
      case VENT_START_WAIT_OPEN: if (now - startupVentTimer >= 1000) { ventServo.writeMicroseconds(SERVO_FORWARD_PULSE); startupVentState = VENT_START_CLOSING_AGAIN; startupVentTimer = now; } break;
      case VENT_START_CLOSING_AGAIN: if (now - startupVentTimer >= SERVO_CLOSE_TIME) { ventServo.writeMicroseconds(SERVO_STOP_PULSE); startupVentState = VENT_START_WAIT_CLOSED_AGAIN; startupVentTimer = now; } break;
      case VENT_START_WAIT_CLOSED_AGAIN: if (now - startupVentTimer >= 1000) { startupVentDone = true; ventState = VENT_CLOSED; displayNeedsUpdate = true; } break;
    }
    yield();
    return;
  }

  unsigned long now = millis();

  // RPM calculation
  if (now - lastRpmMs >= RPM_SAMPLE_MS) {
    noInterrupts();
    unsigned long p = fanPulseCount;
    fanPulseCount = 0;
    interrupts();

    unsigned long elapsedMs = now - lastRpmMs;
    currentFanRPM = (p > 0) ? (p * 60.0f * 1000.0f) / (elapsedMs * 2.0f) : 0.0f;

    lastRpmMs = now;
    displayNeedsUpdate = true;
  }

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
      prefs.putFloat("customTarget", customTarget);
      displayNeedsUpdate = true;
    }
  }

  static int lastBtn = HIGH;
  int btn = digitalRead(ENCODER_BTN);
  if (lastBtn == HIGH && btn == LOW) {
    now = millis();
    if (!inSubMenu && menuIndex != 7) {
      activeMode = menuIndex;
      activeTarget = (activeMode == 5) ? customTarget : menuTargets[activeMode];
      prefs.putInt("activeMode", activeMode);
      inSubMenu = true;
      waitingForSecondSHOW = false;
      lastBtnPressMs = now;

      // Force correct fan state when entering normal mode
      cooldownFanDuty = COOLDOWN_MIN_DUTY;
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
        cooldownStartMs = cooldownLastCheckMs = now;
        cooldownLastTemp = chamberTemp;
        cooldownFanDuty = COOLDOWN_MIN_DUTY;
        setFanDuty(COOLDOWN_MIN_DUTY);
        startOpenVent(false);
      }
      displayNeedsUpdate = true;
    } else {
      if (waitingForSecondSHOW && now - lastBtnPressMs <= DOUBLE_CLICK_MS) {
        inSubMenu = false;
        waitingForSecondSHOW = false;
        updateFan(FAN_OFF);
        startCloseVent();
        displayNeedsUpdate = true;
      } else {
        waitingForSecondSHOW = true;
        lastBtnPressMs = now;
      }
    }
  }
  lastBtn = btn;
  if (waitingForSecondSHOW && millis() - lastBtnPressMs > DOUBLE_CLICK_MS) waitingForSecondSHOW = false;

  if (now - lastDhtReadMs >= DHT_MIN_INTERVAL_MS) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && t > -40 && t < 80) {
      ambientTemp = t;
      ambientHum = isnan(h) ? NAN : (int)round(h);
      displayNeedsUpdate = true;
    }
    lastDhtReadMs = now;
  }

  if (!tempsRequested && now - lastTempRequestMs >= 1000) requestTempsNonBlocking();
  else if (tempsRequested && now - lastTempRequestMs >= DS_CONV_MS) readTempsAfterDelay();

  if (inSubMenu && !isnan(chamberTemp)) {
    // Intake fault detection
    if (!intakeFault && !isnan(intakeTemp) && intakeTemp > chamberTemp + 5.0) {
      intakeFault = true;
      startOpenVent(false);
      updateFan(FAN_HIGH);
      displayNeedsUpdate = true;
    } else if (intakeFault && !isnan(intakeTemp) && intakeTemp <= chamberTemp + 2.0) {
      intakeFault = false;
      displayNeedsUpdate = true;
    }
    if (intakeFault) {
      updateFan(FAN_HIGH);
    } else if (activeMode == 6) {
      // Cooldown mode (unchanged)
      startOpenVent(false);
      float target = isnan(ambientTemp) ? 25.0f : ambientTemp + COOLDOWN_TARGET_OFFSET;
      float total = cooldownLastTemp - target;
      float drop = cooldownLastTemp - chamberTemp;
      cooldownProgress = (total > 0.5f) ? constrain(drop / total, 0.0f, 1.0f) : 1.0f;

      // Fixed estimation - no more constant "Calculating..." on web
      if (cooldownProgress >= 1.0f) {
        cooldownEstSeconds = 0;
      } else if (cooldownLastCheckMs == cooldownStartMs) {
        cooldownEstSeconds = -1;
      } else if (now - cooldownLastCheckMs >= COOLDOWN_SAMPLE_MS) {
        cooldownEstSeconds = 0;
        if (drop > 0.1f && total > 0.5f && (now - cooldownLastCheckMs) > 1000) {
          float rate = drop * (60000.0f / (now - cooldownLastCheckMs));
          if (rate > 0.1f) {
            float rem = chamberTemp - target;
            if (rem > 0) cooldownEstSeconds = (long)(rem / rate * 60.0f);
          }
        }
      }

      if (cooldownLastCheckMs == 0 || isnan(cooldownLastTemp)) {
        cooldownLastCheckMs = now;
        cooldownLastTemp = chamberTemp;
      }
      if (now - cooldownLastCheckMs >= COOLDOWN_SAMPLE_MS) {
        float dpm = drop * (60000.0f / (now - cooldownLastCheckMs));
        int8_t adj = (dpm < COOLDOWN_RATE_DEG_PER_MIN - 0.3f) ? 20 : (dpm > COOLDOWN_RATE_DEG_PER_MIN + 0.3f) ? -20 : 0;
        cooldownFanDuty = constrain(cooldownFanDuty + adj, COOLDOWN_MIN_DUTY, 255);

        if (cooldownFanDuty < COOLDOWN_MIN_DUTY) {
          setFanDuty(0);
        } else {
          setFanDuty(cooldownFanDuty);
        }

        cooldownLastTemp = chamberTemp;
        cooldownLastCheckMs = now;
      }
    } else {
      // NEW SMART DIRECTIONAL HYSTERESIS FOR ALL NORMAL MODES
      if (ventState == VENT_CLOSED) {
        // Stay closed + fan off
        // Only go to half-open if temp rises above target - 1.0°C
        if (chamberTemp > activeTarget + HYSTERESIS_TO_HALF) {
          startOpenVent(true);
          updateFan(FAN_LOW);
        }
        // Can jump directly to full open if significantly hotter
        if (chamberTemp > activeTarget + HYSTERESIS_TO_FULL) {
          startOpenVent(false);
          updateFan(FAN_HIGH);
        }
      } else if (ventState == VENT_HALF_OPEN || ventState == VENT_HALF_OPENING) {
        // Stable half-open + low fan
        // Only leave this state if temp drops below target - 2.0°C (to closed)
        if (chamberTemp < activeTarget + HYSTERESIS_TO_CLOSED) {
          startCloseVent();
          updateFan(FAN_OFF);
        }
        // Or rises above target + 2.0°C (to full open)
        if (chamberTemp > activeTarget + HYSTERESIS_TO_FULL) {
          startOpenVent(false);
          updateFan(FAN_HIGH);
        }
      } else if (ventState == VENT_OPEN || ventState == VENT_OPENING) {
        // Full open + high fan
        // Only drop back to half-open when temp falls to or below target + 1.0°C
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

  processVentState();
  updateLED();
  server.handleClient();
  yield();

  if (displayNeedsUpdate && millis() - lastOledMs >= OLED_INTERVAL_MS) {
    lastOledMs = millis();
    inSubMenu ? drawSubMenu() : drawMainMenu();
  }
}
