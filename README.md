<div align="center">

# **Chamber Master (v2.5)**

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
