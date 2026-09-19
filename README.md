<div align="center">

# **Chamber Master (v2.9-beta)**

**ESP32-based Smart 3D Printer Enclosure Controller**

<img width="3872" height="1984" alt="Chamber Master Enclosure Setup" src="https://github.com/user-attachments/assets/fdeca6c7-7432-427c-bc56-d5b119a349c6" />

### 🎥 Watch the Video Demo
[![Watch the video](https://img.youtube.com/vi/ktXHP1pz5N8/0.jpg)](https://www.youtube.com/watch?v=ktXHP1pz5N8)

</div>

---

**Chamber Master** is an intelligent, ESP32-powered environmental controller for 3D printer enclosures. It features precise multi-zone temperature regulation, automated vent shutter control using directional hysteresis, dynamic 4-pin PC fan PWM control with low-side hard kill zero-RPM cut-off, adaptive material cooldown routines (for ABS/ASA thermal stress prevention), intake fault safety alerts, an OLED display UI, and a responsive Web Dashboard with mDNS support (`http://enclosure-monitor.local`).

---

## 🌟 Key Features

### 🌡️ Temperature Monitoring
![temp](https://github.com/user-attachments/assets/471e7b9d-16c5-4621-9443-8cb17a5fd830)
- **Chamber Temperature:** DS18B20 OneWire sensor placed inside the enclosure.
- **Intake Air Temperature:** Second DS18B20 placed on the printer's motherboard vent or fresh air intake path (essential for recirculation fault detection).
- **Ambient Environment:** DHT11 (or DHT22) for room temperature and relative humidity sensing.

### 🚪 Smart Vent Control
![vent](https://github.com/user-attachments/assets/e6650e52-f200-4bac-ad4d-a0612e5d8f0b)
- SG90 micro servo driving vent shutter with **three precise states**: Closed • Half-Open • Full-Open.
- Advanced **directional hysteresis logic** for rock-solid temperature stability (eliminates mechanical servo chatter and hunting).
- Servos perform an automated calibration cycle on boot for accurate homing.

### 💨 Fan Control & Hard Kill Transistor
- Standard 4-pin PC fan with 1 kHz PWM speed control and tachometer ISR feedback (real RPM displayed on OLED & web).
- Enforced **minimum 20% duty cycle (~51/255)** during active cooling and cooldown to prevent thermal shock.
- **2N2222 Low-Side Hard Kill Transistor:** Cuts the fan's Ground (GND) line on GPIO 15 when fan is OFF, overcoming the Intel 4-wire PC fan specification failsafe to achieve true 0 RPM.

### 🎯 Operating Modes
- **Material Presets:**
  - **PLA:** 30.0°C
  - **ASA:** 50.0°C
  - **ABS:** 60.0°C
  - **TPU:** 25.0°C
  - **PETG:** 40.0°C
- **Custom Mode:** User-selectable target (0.0°C to 120.0°C, 0.5°C step via encoder, debounced & saved persistently to NVS flash).
- **Adaptive Cooldown Mode:**
  - Starts at 20% fan speed + full vent open (vent opens before fan to avoid back-pressure noise).
  - Automatically adjusts fan speed to achieve ~1.5°C/min cooling rate.
  - Live progress bar + real-time countdown timer tick on OLED and Web Dashboard.

### 🛡️ Safety Features
- **Intake Fault Detection:** Triggers emergency max fan + full vent if intake air exceeds `Chamber Temp + 5.0°C` (prevents hot air recirculation into electronics).
- Automatic recovery banner clearing when fault condition resolves.

### 🖥️ User Interface (OLED & Encoder)
![oled menu](https://github.com/user-attachments/assets/79ddc128-01dc-4299-95bf-ec3f79ad745c)
- Crisp **SSD1306 128×64 OLED** display with blinking chamber temperature status.
- Intuitive **EC11 rotary encoder** with 50ms software debounced push button.
- **Double-Click** encoder button to safely exit active mode (closes vent, turns fan off).
- **QR Code Mode:** Generates pairing QR Code and text string for quick mobile access.

### 🌐 Responsive Web Dashboard
![webpage gif](https://github.com/user-attachments/assets/7d7d6779-34f5-4f4d-867a-c52affe4427c)
- Dark glassmorphic design with real-time 1-second background telemetry updates.
- Displays all sensor readings, fan speed/RPM, vent state, active mode & target temp.
- CSS animated spinning fan blades with dynamic RPM rotation speed.
- Cooldown progress ring + remaining time estimate.
- One-click **START COOLDOWN** remote button.
- Live printer camera iframe (`http://3d-print-live.local/`).
- Accessible on local network at **`http://enclosure-monitor.local`** (via mDNS).

---

## 📌 Hardware Pinout Diagram

| Hardware Module | ESP32 GPIO | Description |
| :--- | :--- | :--- |
| **SSD1306 OLED SDA** | GPIO 21 | I2C Data |
| **SSD1306 OLED SCL** | GPIO 22 | I2C Clock |
| **Chamber DS18B20** | GPIO 32 | OneWire Sensor Bus |
| **Intake DS18B20** | GPIO 13 | OneWire Sensor Bus |
| **Ambient DHT11** | GPIO 23 | Temperature & Humidity Data |
| **Fan PWM Signal** | GPIO 33 | 4-Pin Fan PWM Pin (1 kHz) |
| **Fan Tachometer** | GPIO 19 | Pulse ISR Input (Debounced) |
| **Hard Kill Transistor** | GPIO 15 | 2N2222 Base Pin (via 1kΩ Resistor) |
| **Vent Servo PWM** | GPIO 5 | SG90 Control Pulse |
| **Encoder CLK / DT / BTN**| GPIO 25 / 26 / 27 | Quadrature Encoder & Push Button |
| **Status LED** | GPIO 2 | High during active cooling/venting |

---

## 📁 Codebase Architecture

The project supports both single-file Arduino IDE workflows and PlatformIO multi-file modular builds:

- **Arduino IDE (Single-File / Tab Setup):**
  - **[`Chamber-Master.ino`](file:///c:/Users/JB/Documents/chamber%20master/Chamber-Master.ino):** Main standalone sketch containing full controller logic.
  - **[`config.h`](file:///c:/Users/JB/Documents/chamber%20master/config.h):** Separate tab for Wi-Fi credentials (`WIFI_SSID`, `WIFI_PASSWORD`, `MDNS_HOSTNAME`).

- **PlatformIO / VS Code (Modular Build):**
  - `include/`: Header declarations (`config.h`, `sensors.h`, `actuators.h`, `cooldown.h`, `ui_display.h`, `web_dashboard.h`).
  - `src/`: C++ module implementations (`main.cpp`, `config.cpp`, `sensors.cpp`, `actuators.cpp`, `cooldown.cpp`, `ui_display.cpp`, `web_dashboard.cpp`).

---

## 🖨️ Klipper Integration & Macros (v2.9-beta)

> *Hardware Tested & Verified: Klipper integration and macros proposed and verified on physical hardware by [Richard Kennett](https://github.com/richard-kennett) ([Issue #3](https://github.com/jayanttyson/Chamber-Master/issues/3#issuecomment-5619889047)).*

Chamber Master exposes a REST API endpoint (`/material`) allowing Klipper 3D printer firmware to automatically configure enclosure target temperatures and material profiles directly from filament start G-code or slicer profiles. Complete ready-to-use configuration files are located in the [`Klipper/`](Klipper/) directory.

### 1. Prerequisites on Klipper Host
Install mDNS resolution support on your Klipper host (Raspberry Pi / Linux):
```bash
sudo apt update && sudo apt install -y mdns avahi-daemon
```

### 2. Install Host Trigger Script
Copy [`Klipper/chamber_trigger.sh`](Klipper/chamber_trigger.sh) to your home directory (`$HOME`):
```bash
cp Klipper/chamber_trigger.sh ~/chamber_trigger.sh
chmod +x ~/chamber_trigger.sh
```
*Note: The script performs an IPv4 ping lookup (`ping -4 -c 1`) to cache and grab the controller's IP immediately, preventing curl DNS resolution timeouts in Klipper subshells.*

### 3. Klipper Macro Configuration
Add the tested macros below to your `printer.cfg` (or add `[include chamber_master.cfg]` using [`Klipper/chamber_master.cfg`](Klipper/chamber_master.cfg)):

```ini
[gcode_macro Chamber_Master]
description: Configuration variables for Chamber Master
variable_url: "enclosure-monitor.local"   # Matches mDNS hostname on OLED screen
gcode:

[gcode_shell_command chamber_curl]
command: bash $HOME/chamber_trigger.sh
timeout: 4.0
verbose: True

[gcode_macro SET_CHAMBER]
description: Set Chamber Master material mode and optional target temperature
gcode:
    {% set cf = printer.configfile.settings %}
    {% set url = cf['gcode_macro chamber_master'].variable_url %}

    # 1. Read material and force it to UPPERCASE
    {% set material = params.MATERIAL|default("XXX")|string|upper %}
    {% if material == "" %}
        {% set material = "XXX" %}
    {% endif %}

    # 2. Check if temperature was explicitly passed
    {% if 'TEMPERATURE' in params %}
        {% set temperature = params.TEMPERATURE|int %}
        RUN_SHELL_COMMAND CMD=chamber_curl PARAMS="{url} {material} {temperature}"
    {% else %}
        RUN_SHELL_COMMAND CMD=chamber_curl PARAMS="{url} {material}"
    {% endif %}

[gcode_macro START_CHAMBER_COOLDOWN]
description: Trigger Chamber Master adaptive cooldown routine
gcode:
    SET_CHAMBER MATERIAL=COOLDOWN
```

### 4. Slicer Filament Start G-Code Examples
In OrcaSlicer, PrusaSlicer, or Bambu Studio, add to your **Filament Start G-Code**:
```gcode
; Automate enclosure temperature for active filament
SET_CHAMBER MATERIAL=[filament_type]
```
For custom chamber temperatures:
```gcode
SET_CHAMBER MATERIAL=CUSTOM TEMPERATURE=55
```

### 5. Print End Cooldown
In your **Machine End G-Code** or `PRINT_END` macro:
```gcode
START_CHAMBER_COOLDOWN
```

---

## 🛠️ Setup & Installation

### Option A: Arduino IDE (Easiest)

1. **Add ESP32 Board Support:**
   - Go to **File → Preferences**
   - Add to *Additional Boards Manager URLs*: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - Go to **Tools → Board → Boards Manager**, search **esp32**, and click Install.

2. **Install Required Libraries (Tools → Manage Libraries):**
   - `Adafruit SSD1306`
   - `Adafruit GFX Library`
   - `OneWire`
   - `DallasTemperature`
   - `DHT sensor library` (by Adafruit)
   - `ESP32Encoder` (by Kevin Harrington)
   - `ESP32Servo` (by Kevin Harrington)
   - `QRCodeGFX` (by Ricmoo)

3. **Configure & Upload:**
   - Open [`Chamber-Master.ino`](file:///c:/Users/JB/Documents/chamber%20master/Chamber-Master.ino) in Arduino IDE.
   - Click the **`config.h`** tab at the top and set your Wi-Fi credentials:
     ```cpp
     #define WIFI_SSID     "YOUR_WIFI_NAME"
     #define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
     ```
   - Select Board **Tools → Board → ESP32 Dev Module**.
   - Connect your ESP32 and click **Upload**.

---

### Option B: PlatformIO (VS Code)

1. Open this workspace folder in VS Code with the PlatformIO extension installed.
2. Update Wi-Fi settings in `config.h`.
3. Connect your ESP32 via USB and run:
   ```bash
   pio run --target upload
   ```

---

## 💡 Pro Tips for Best Results

- **Intake Sensor Placement:** Place the intake DS18B20 directly at the fresh air entry point or near printer motherboard exhaust vents for early recirculation detection.
- **Fan Choice:** Choose a high-quality ≥2000 RPM 4-pin PWM fan (e.g., Noctua or Arctic) for silent yet effective airflow.
- **Vent Grease:** Apply a small amount of grease to gears and rotating flaps in the aperture vent mechanism for smooth operation.
- **Enclosure Material:** For ABS/ASA printing, pair with a well-sealed enclosure (IKEA Lack, Prusa enclosure, or custom build).

---

## 📄 License

MIT License. Free to use, modify, and distribute. Created by **Jayant Bhatia**.
