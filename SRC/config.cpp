/*
 * Chamber Master - 3D Printer Enclosure Controller
 * Global Variables & Configuration Storage Definitions
 */

#include "config.h"

// SSD1306 Display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
QRCodeGFX qrcode(display);

// Fan PWM
const int FAN_PWM_FREQ = 1000;
const int FAN_PWM_RES  = 8;
volatile uint8_t fanDutyCycle = 0;
ESP32PWM fanPWM;
const uint8_t NORMAL_MIN_DUTY = 51; // ~20%

// RPM Measurement
volatile unsigned long fanPulseCount = 0;
volatile unsigned long lastFanPulseUs = 0;
unsigned long lastRpmMs = 0;
const unsigned long RPM_SAMPLE_MS = 1000;
float currentFanRPM = 0.0f;

// Temperature Sensors
OneWire oneWireChamber(CHAMBER_PIN);
DallasTemperature chamberSensor(&oneWireChamber);
OneWire oneWireIntake(INTAKE_PIN);
DallasTemperature intakeSensor(&oneWireIntake);
DHT dht(DHT_PIN, DHTTYPE);

// Rotary Encoder & Menu
ESP32Encoder encoder;
long lastEncoderCount = 0;

const char *menuItems[] = {"PLA", "ASA", "ABS", "TPU", "PETG", "CUSTOM", "COOLDOWN", "QR CODE"};
const float menuTargets[] = {30.0f, 50.0f, 60.0f, 25.0f, 40.0f, -1.0f, -2.0f, -3.0f};
const int MENU_LEN = 8;

int menuIndex = 0;
bool inSubMenu = false;
int activeMode = 0;
float customTarget = 30.0f;
float activeTarget = 30.0f;

// NVS Preferences
Preferences prefs;

// Display & Timing
unsigned long lastOledMs = 0;
const unsigned long OLED_INTERVAL_MS = 200;

unsigned long lastTempRequestMs = 0;
const unsigned long DS_CONV_MS = 750;
bool tempsRequested = false;

unsigned long lastDhtReadMs = 0;
const unsigned long DHT_MIN_INTERVAL_MS = 3500;

float chamberTemp = NAN;
float intakeTemp = NAN;
float ambientTemp = NAN;
float ambientHum = NAN;

bool displayNeedsUpdate = true;

// Vent Servo
Servo ventServo;
VentState ventState = VENT_CLOSED;
unsigned long ventActionStartMs = 0;

const unsigned long SERVO_OPEN_TIME = 850;
const unsigned long SERVO_HALF_OPEN_TIME = 425;
const unsigned long SERVO_CLOSE_TIME = 1100;
const unsigned long SERVO_FULL_TO_HALF_TIME = 750;

const int SERVO_STOP_PULSE = 1500;
const int SERVO_FORWARD_PULSE = 2000;  // Close vent
const int SERVO_REVERSE_PULSE = 1000;  // Open vent

// Fan State & Hysteresis
FanSpeed fanSpeed = FAN_OFF;
bool ledOn = false;

const float HYSTERESIS_TO_HALF = -1.0f;
const float HYSTERESIS_TO_CLOSED = -2.0f;
const float HYSTERESIS_TO_FULL = 2.0f;
const float HYSTERESIS_FROM_FULL = 1.0f;

// UI & Button
bool chamberTempVisible = true;
unsigned long lastBlinkMs = 0;
const unsigned long BLINK_INTERVAL_MS = 500;

unsigned long lastBtnPressMs = 0;
const unsigned long DOUBLE_CLICK_MS = 250;
bool waitingForSecondSHOW = false;
unsigned long lastBtnDebounceMs = 0;
const unsigned long BTN_DEBOUNCE_MS = 50;

unsigned long lastCustomChangeMs = 0;
bool customTargetDirty = false;

// WiFi Credentials
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
WebServer server(80);

// Cooldown Logic
float cooldownStartTemp = 0.0f;
float cooldownLastTemp = 0.0f;
unsigned long cooldownLastCheckMs = 0;
uint8_t cooldownFanDuty = NORMAL_MIN_DUTY;

const float COOLDOWN_RATE_DEG_PER_MIN = 1.5f;
const float COOLDOWN_TARGET_OFFSET = 3.0f;
const unsigned long COOLDOWN_SAMPLE_MS = 60000;

unsigned long cooldownStartMs = 0;
long cooldownEstSeconds = -1;
float cooldownProgress = 0.0f;
unsigned long lastCountdownSecMs = 0;

// Safety & Startup Vent
bool intakeFault = false;
StartupVentState startupVentState = VENT_START_CLOSING;
unsigned long startupVentTimer = 0;
bool startupVentDone = false;
