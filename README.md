# Chamber-Master
ESP32-based smart 3D printer enclosure controller with precise temperature regulation, adaptive vent/fan control using directional hysteresis, intake fault safety, adaptive cooldown mode, OLED menu with rotary encoder, and a responsive web dashboard for monitoring and control.


Key FeaturesTemperature MonitoringChamber: DS18B20 inside the enclosure
Intake air: Second DS18B20 on the fresh air path (critical for fault detection)
Ambient: DHT11 for room temperature & humidity

Smart Vent ControlSG90 micro servo with three precise states: Closed, Half-Open, Full-Open
Advanced directional hysteresis logic for rock-solid temperature stability (no oscillating)

Fan ControlStandard 4-pin PC fan with full PWM and tachometer feedback (real RPM on display & web)
Enforced minimum 20% duty during operation/cooldown to avoid sudden thermal shock
Hard-kill transistor (2N2222) for true 0 RPM when off – no standby spin

Operating ModesMaterial presets: PLA (30°C), ASA (50°C), ABS (60°C), TPU (25°C), PETG (40°C)
Custom mode (0–120°C, saved persistently)
Adaptive Cooldown Mode:Starts at 20% fan + full vent open
Auto-adjusts fan speed for ~1.5°C/min cooling
Targets ambient + 3°C
Live progress bar + estimated time remaining

Safety FeaturesIntake fault detection: Emergency max cooling if intake air > chamber +5°C (prevents hot air recirculation)
Automatic recovery when cleared

User InterfaceCrisp SSD1306 128x64 OLED
Intuitive rotary encoder navigation + push button
Double-click to exit active mode (safely closes vent & stops fan)
Blinking chamber temp for quick glance
QR code menu item for instant web access

Web Dashboard
Sleek dark glass morphism design with live updates
All sensors, fan RPM/%, vent state, mode, target
Animated spinning fan icon
Cooldown progress ring + time estimate
Flashing red banner on faults
One-click cooldown start
Built-in iframe for your printer cam (e.g., http://3d-print-live.local)
mDNS access: http://enclosure-monitor.local

ExtrasPersistent settings (last mode & custom target)
Startup servo calibration for perfect positioning
Built-in LED shows active cooling
DS18B20 sensor mounts (examples)

Hardware RequirementsComponent
Recommended / Notes
ESP32 dev board
Any (e.g., ESP32-WROOM-32)
SSD1306 128x64 OLED
I2C, 0x3C address
2× DS18B20 sensors
Waterproof versions ideal for enclosure
DHT11 (or DHT22 for accuracy)
Ambient temp/humidity
Rotary encoder + button
Standard KY-040 style
SG90 micro servo
For vent flap
120mm 4-pin PC fan
PWM + tachometer
2N2222 transistor + 1kΩ resistor
Hard-kill low-side switch on fan GND
Power supplies
5V for logic/servo; 12V for fan if needed

Pinout (as coded – easily changeable):Servo: 5 • Fan PWM: 33 • Fan Tach: 19 • Fan Power Kill: 15
Chamber DS18B20: 32 • Intake DS18B20: 13 • DHT: 23
Encoder: 25/26/27 • OLED: I2C (default SDA/SCL)

Setup & InstallationLibraries (Arduino IDE → Library Manager):Adafruit SSD1306 & GFX
OneWire
DallasTemperature
DHT sensor library
ESP32Encoder
ESP32Servo
QRCodeGFX

Upload:Open the .ino file
Fill in your WiFi credentials
Select ESP32 board & upload

First Run:Servo runs a quick calibration cycle
Use encoder to pick a mode → press to activate
Web dashboard: http://enclosure-monitor.local

Pro Tips for Best ResultsPlace the intake DS18B20 right at the fresh air entry (or near printer motherboard vents) to catch recirculation early.
Fine-tune servo timings in code to match your printed vent mechanism.
Use a quiet 120mm Noctua fan for near-silent operation.
For ABS/ASA, combine with a good enclosure (IKEA Lack, Prusa, or custom).
The cooldown algorithm works best with decent airflow—add intake holes if needed.

ContributingLove it? Help make it better!Report bugs or suggest features via Issues
PRs welcome: new UI tweaks, more material presets, schematics, 3D models for mounts/cases
Share your builds!

LicenseMIT License – free to use, modify, and share.Happy printing! Version 1.1 • December 19, 2025


