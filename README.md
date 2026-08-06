# 3D Printer Enclosure Controller (v2.5)

An ESP32-powered environmental controller for 3D printer enclosures featuring multi-zone thermal sensing, automated vent shutter control, 4-pin PC fan PWM control with low-side hard kill zero-RPM cut-off, adaptive material cooldown routines, intake fault safety alerts, an OLED display UI, and a responsive Web Dashboard with mDNS support (`http://enclosure-monitor.local`).

---

## Key Features

- **Multi-Sensor Thermal Monitoring:**
  - DS18B20 OneWire sensor for chamber temperature.
  - DS18B20 OneWire sensor for air intake temperature.
  - DHT11 sensor for external ambient temperature and humidity.
- **Ventilation & Exhaust Control:**
  - SG90 servo motor driving vent shutter with microsecond pulse timing.
  - Directional hysteresis logic to prevent servo chatter and mechanical wear.
  - 4-pin PC Fan PWM speed control (1 kHz, 8-bit resolution).
  - Low-side 2N2222 transistor hard kill ground cut-off for true 0 RPM operation.
- **Adaptive Material Cooldown Routine:**
  - Prevents thermal shock and cracking in technical materials (ABS, ASA).
  - Enforces minimum 20% fan speed while active.
  - Dynamically calculates remaining cooldown time and updates countdown timer in real time.
- **Safety Features:**
  - Intake fault detection (`Intake Temp > Chamber Temp + 5.0°C`) triggers max emergency cooling and visual alert banners.
- **User Interface:**
  - SSD1306 128x64 OLED display with EC11 rotary encoder menu navigation.
  - Mobile-responsive Web Dashboard accessible at `http://enclosure-monitor.local`.
  - QR Code display mode for mobile device quick-pairing.

---

## Hardware Pinout Diagram

| Hardware Module | ESP32 GPIO | Description |
| :--- | :--- | :--- |
| **SSD1306 OLED SDA** | GPIO 21 | I2C Data |
| **SSD1306 OLED SCL** | GPIO 22 | I2C Clock |
| **Chamber DS18B20** | GPIO 32 | OneWire Sensor Bus |
| **Intake DS18B20** | GPIO 13 | OneWire Sensor Bus |
| **Ambient DHT11** | GPIO 23 | Temperature & Humidity Data |
| **Fan PWM Signal** | GPIO 33 | 4-Pin Fan PWM Pin |
| **Fan Tachometer** | GPIO 19 | Pulse ISR Input |
| **Hard Kill Transistor** | GPIO 15 | 2N2222 Base Pin (via 1kΩ Resistor) |
| **Vent Servo PWM** | GPIO 5 | SG90 Control Pulse |
| **Encoder CLK / DT / BTN**| GPIO 25 / 26 / 27 | Quadrature Encoder & Push Button |
| **Status LED** | GPIO 2 | High during active cooling/venting |

---

## Codebase Architecture

The project supports both VS Code/PlatformIO modular builds and Arduino IDE single-file builds:

- **Arduino IDE Sketch Setup:**
  - `Chamber-Master.ino`: Main sketch file containing all controller firmware logic.
  - `config.h`: Separate tab for setting your Wi-Fi SSID, Wi-Fi password, and mDNS hostname without cluttering main logic or leaking passwords in commits.

---

## Building & Flashing in Arduino IDE

1. Open `Chamber-Master.ino` in Arduino IDE.
2. Click on the **`config.h`** tab at the top.
3. Edit your Wi-Fi credentials:
   ```cpp
   #define WIFI_SSID     "YOUR_WIFI_NAME"
   #define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
   ```
4. Select board: **Tools → Board → ESP32 Dev Module**.
5. Click **Upload**.

---

## Web Dashboard & API

Access the Web Dashboard in any browser at `http://enclosure-monitor.local`.

- **`GET /`**: Main control dashboard with real-time sensor gauges, spinning fan animation, camera feed iframe, and cooldown controls.
- **`GET /status`**: JSON endpoint providing system telemetry.
- **`POST /start_cooldown`**: Remotely triggers adaptive cooldown mode.

---

## License

MIT License. Free to use, modify, and distribute. Created by Jayant Bhatia.
