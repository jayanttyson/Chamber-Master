![chamber master](https://github.com/user-attachments/assets/1aa07578-f1d4-4f93-a6e0-61c9c6e856b4)


**ESP32-based smart 3D printer enclosure controller** with precise temperature regulation, adaptive vent/fan control using directional hysteresis, intake fault safety, adaptive cooldown mode, OLED menu with rotary encoder, and a responsive web dashboard for monitoring and control.

## Key Features

### Temperature Monitoring
- **Chamber**: DS18B20 placed inside the enclosure
- **Intake air**: Second DS18B20 on the fresh air intake path (essential for fault detection)
- **Ambient**: DHT11 (or DHT22) for room temperature and humidity

### Smart Vent Control
- SG90 micro servo with **three precise states**: Closed • Half-Open • Full-Open
- Advanced **directional hysteresis logic** for rock-solid temperature stability (eliminates oscillation)

### Fan Control
- Standard 4-pin PC fan with full PWM control and tachometer feedback (real RPM shown on OLED & web)
- Enforced **minimum 20% duty** during operation and cooldown to prevent thermal shock
- **Hard-kill transistor** (2N2222 + 1kΩ resistor) for true 0 RPM when off – no standby spin

### Operating Modes
- **Material presets**:
  - PLA → 30°C
  - ASA → 50°C
  - ABS → 60°C
  - TPU → 25°C
  - PETG → 40°C
- **Custom mode**: Adjustable 0–120°C (saved persistently)
- **Adaptive Cooldown Mode**:
  - Starts at 20% fan speed + full vent open
  - Automatically adjusts fan speed to achieve ≈1.5°C/min cooling rate
  - Targets ambient temperature + 3°C
  - Live progress bar + estimated time remaining on both OLED and web dashboard

### Safety Features
- **Intake fault detection**: Triggers emergency max fan + full vent if intake air > chamber +5°C (prevents hot air recirculation)
- Automatic recovery when fault clears

### User Interface
- Crisp **SSD1306 128×64 OLED** display
- Intuitive **rotary encoder** with push button for navigation
- **Double-click** encoder button to safely exit active mode (closes vent, stops fan)
- Blinking chamber temperature for quick visibility
- QR code menu item for instant web dashboard access

### Web Dashboard
- Sleek **dark glassmorphism** design with live 1-second updates
- Displays all sensors, fan RPM/%, vent state, current mode & target
- Animated spinning fan icon
- Cooldown progress ring + estimated time
- Flashing red banner during intake faults
- One-click button to start cooldown
- Built-in iframe for live printer camera (e.g., `http://3d-print-live.local` – basic ESP32-CAM sketch coming soon)
- Accessible via mDNS: **http://enclosure-monitor.local**

### Extras
- Persistent settings (last selected mode & custom target) using NVS
- Startup servo calibration routine for reliable homing and positioning
- Built-in LED (GPIO 2) indicates active cooling
- Planned: 3D-printable mounts and cases (STL files coming soon)

## Hardware Requirements

| Component                          | Recommended / Notes                                                                 |
|------------------------------------|-------------------------------------------------------------------------------------|
| ESP32 development board            | Any standard board (e.g., ESP32-WROOM-32)                                           |
| SSD1306 128×64 OLED display        | I2C interface, address 0x3C                                                         |
| 2× DS18B20 temperature sensors     | Waterproof recommended; optional 2.2kΩ pull-up resistor for long cable runs         |
| DHT11 or DHT22                     | For ambient temperature & humidity (DHT22 more accurate)                            |
| Rotary encoder with button         | Standard KY-040 or equivalent                                                       |
| SG90 micro servo                   | Standard rotation (not continuous) – for vent flap actuation                        |
| 120mm 4-pin PC fan                 | Must support PWM control + tachometer output                                        |
| 2N2222 transistor + 1kΩ resistor   | Low-side hard-kill switch on fan ground line                                        |
| Power supply                       | 5V for ESP32/servo/sensors; 12V for fan (can be sourced from printer PSU)           |

### Default Pinout (easily changeable in code)
- Servo: **GPIO 5**
- Fan PWM: **GPIO 33**
- Fan Tachometer: **GPIO 19**
- Fan Power Kill: **GPIO 15**
- Chamber DS18B20: **GPIO 32**
- Intake DS18B20: **GPIO 13**
- DHT11/22: **GPIO 23**
- Encoder CLK/DT/SW: **GPIO 25 / 26 / 27**
- OLED: Standard I2C (SDA = GPIO 21, SCL = GPIO 22 on most boards)

## Setup & Installation

### Required Libraries (Arduino IDE → Library Manager)
- Adafruit SSD1306
- Adafruit GFX
- OneWire
- DallasTemperature
- DHT sensor library (by Adafruit)
- ESP32Encoder
- ESP32Servo
- QRCodeGFX

### Upload Instructions
1. Open `Chamber-Master.ino` in Arduino IDE
2. Edit WiFi credentials:

   const char *ssid = "YOUR_SSID";
   const char *password = "YOUR_PASSWORD";
