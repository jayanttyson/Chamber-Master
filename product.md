# Product Requirement & Specification Document (product.md)

## Project Overview
**Product Name:** 3D Printer Enclosure Controller  
**Version:** 2.5 (Production-Grade Enhanced)  
**Author:** Jayant Bhatia  
**License:** MIT License  

The **3D Printer Enclosure Controller** is an intelligent, embedded ESP32 system designed for monitoring and controlling environmental conditions within a 3D printer enclosure. It provides real-time thermal monitoring across multiple zones, automated servo-driven ventilation control, dynamic 4-pin PC fan PWM management with low-side hard kill zero-RPM control, adaptive material cooldown routines (ABS/ASA thermal stress prevention), intake fault safety protection, rotary encoder UI with OLED display, and a modern responsive web dashboard accessible via mDNS (`http://enclosure-monitor.local`).

---

## Codebase Architecture (v2.5 Modular Structure)

The codebase supports **both** a clean multi-module architecture (PlatformIO / VS Code) and a single-file legacy format (`Chamber-Master.ino`):

```
chamber master/
├── Chamber-Master.ino      # Main standalone sketch (Arduino IDE)
├── config.h                # WiFi SSID, Password & Hostname configuration tab
├── README.md               # Hardware pinouts & setup guide
└── product.md              # Specification document
```

---

## Hardware Specifications & Pinout Mapping

| Component | Interface / Type | ESP32 GPIO Pin | Notes |
| :--- | :--- | :--- | :--- |
| **SSD1306 OLED (128x64)** | I2C (Address 0x3C) | SDA: GPIO 21, SCL: GPIO 22 | Main UI Display |
| **Chamber Temp Sensor** | DS18B20 (OneWire) | GPIO 32 | Internal enclosure temperature |
| **Intake Temp Sensor** | DS18B20 (OneWire) | GPIO 13 | Air intake temperature |
| **Ambient Temp & Humidity** | DHT11 | GPIO 23 | External ambient environment |
| **Fan PWM Control** | 4-Pin PWM Fan | GPIO 33 | 1 kHz, 8-bit resolution |
| **Fan Tachometer Input** | Pulse Frequency ISR | GPIO 19 | Measures fan RPM |
| **Fan Hard Kill Transistor** | 2N2222 Low-side switch | GPIO 15 | True 0 RPM ground cut-off (overrides CPU fan failsafe) |
| **Vent Servo Control** | SG90 / Continuous Servo | GPIO 5 | Microsecond pulse control |
| **Rotary Encoder CLK** | EC11 Encoder | GPIO 25 | Quadrature decoder |
| **Rotary Encoder DT** | EC11 Encoder | GPIO 26 | Quadrature decoder |
| **Rotary Encoder Button** | Push Button | GPIO 27 | Active LOW with internal pullup |
| **Status LED Indicator** | Onboard/External LED | GPIO 2 | High when fan & vent active |

---

## Operating Modes

1. **PLA Mode:** Target Chamber Temp = 30.0°C (Prevents heat creep & nozzle clogs).
2. **ASA Mode:** Target Chamber Temp = 50.0°C (Maintains chamber warmth for warping prevention).
3. **ABS Mode:** Target Chamber Temp = 60.0°C (High ambient heat retention for structural integrity).
4. **TPU Mode:** Target Chamber Temp = 25.0°C (Maximum active cooling for flexible filament).
5. **PETG Mode:** Target Chamber Temp = 40.0°C (Balanced thermal environment).
6. **CUSTOM Mode:** User selectable target (0.0°C to 120.0°C, 0.5°C step via encoder saved to NVS).
7. **COOLDOWN Mode:** Adaptive cooling routine. Starts at 20% fan duty (~51/255) to prevent thermal shock, adjusts fan duty dynamically based on temperature drop rate (°C/min), and closes vent upon completion. Progress tracked from original start temperature (v2.5 fix).
8. **QR CODE Mode:** Displays QR Code and URL string (`http://enclosure-monitor.local`) for quick mobile dashboard pairing.

---

## v2.5 Production Fixes

| # | Category | Fix |
| :--- | :--- | :--- |
| 1 | **Bug** | Cooldown progress bar now tracks from original start temp, not rolling 60s window |
| 2 | **Bug** | EC11 encoder button 50ms debounce eliminates phantom double-clicks |
| 3 | **Bug** | Button handler uses isolated timestamp (`btnNow`) preventing mid-loop timing drift |
| 4 | **Bug** | Cooldown start guards against NaN chamber temp (falls back to 60°C) |
| 5 | **Improvement** | NVS flash writes debounced to 2s idle on custom target encoder scrolling |
| 6 | **Improvement** | WiFi hostname changed from `"ENCLOSURE MONITOR"` to `"enclosure-monitor"` (RFC 952) |
| 7 | **Improvement** | `/status` endpoint includes `Access-Control-Allow-Origin: *` CORS header |
| 8 | **Improvement** | Unknown web routes return proper 404 via `server.onNotFound()` |
| 9 | **Improvement** | Vent opens before fan engages in cooldown to prevent back-pressure noise |
| 10 | **Cleanup** | Removed unnecessary `(int)` cast on `ambientHum` assignment (`roundf()` instead) |

---

## Hardware Rationale: 4-Pin CPU Fan Failsafe & Hard Kill Transistor

Standard 4-pin PC cooling fans adhere to the **Intel 4-Wire PWM Fan Specification**. As a hardware CPU protection feature:
- Many PC fans treat a **0% PWM signal** or disconnected PWM wire as an emergency state, keeping the fan spinning at a **minimum fallback speed (~20% RPM, ~500–800 RPM)** so a CPU does not overheat if the PWM controller fails.
- Therefore, driving 0% duty on GPIO 33 alone fails to turn off the fan on hardware-failsafe PC fans.
- The **2N2222 low-side transistor** circuit on GPIO 15 physically cuts the fan's Ground (GND) rail when `FAN_OFF` is commanded, guaranteeing **true 0 RPM operation** and eliminating lingering motor hum/spin.

---

## Control Logic & State Management

### 1. Vent Servo Control
- **Closed:** Continuous pulse = 2000 µs (SERVO_FORWARD_PULSE).
- **Open:** Continuous pulse = 1000 µs (SERVO_REVERSE_PULSE).
- **Stop:** Pulse = 1500 µs (SERVO_STOP_PULSE).
- **Hysteresis Logic:**
  - `Temp > Target + 2.0°C`: Full Open Vent + FAN_HIGH (100% duty).
  - `Temp > Target - 1.0°C`: Half Open Vent + FAN_LOW (55% duty).
  - `Temp < Target - 2.0°C`: Close Vent + FAN_OFF (0% duty, Hard Kill active).

### 2. Fan & Hard Kill Transistor Operation
- **FAN_OFF / 0 Duty:** GPIO 15 goes `LOW`, turning off the 2N2222 transistor to disconnect the fan GND rail. `fanPWM.writeScaled(0.0f)` sets PWM to 0.
- **FAN_LOW / FAN_HIGH / Active Cooldown:** GPIO 15 goes `HIGH`, connecting fan GND rail. PWM duty cycle regulates fan speed.

### 3. Intake Fault Protection
- If `Intake Temp > Chamber Temp + 5.0°C`, system triggers `INTAKE FAULT`.
- Vent immediately opens fully, fan runs at `FAN_HIGH` (100%), OLED shows `INTAKE FAULT!`, and Web Dashboard displays a blinking alert banner.
- Clears automatically when `Intake Temp <= Chamber Temp + 2.0°C`.

---

## Web Dashboard & REST API Specs

- **GET `/`**: Responsive HTML/CSS Web Dashboard with glassmorphism design, real-time stat cards, spinning fan SVG animation, circular cooldown progress gauge, live camera iframe embed (`http://3d-print-live.local/`), and start cooldown button.
- **GET `/status`**: Returns JSON object:
  ```json
  {
    "chamberTemp": 42.5,
    "intakeTemp": 31.0,
    "ambientTemp": 24.2,
    "ambientHum": 45,
    "fanRpm": 2100,
    "fanDuty": 55,
    "activeMode": "ABS",
    "targetTemp": "60.0",
    "ventState": "HALF OPEN",
    "fault": "NONE",
    "estSeconds": -1,
    "progress": 0.0
  }
  ```
- **POST `/start_cooldown`**: Initiates adaptive cooldown mode from web interface.
