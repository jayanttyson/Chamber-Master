/*
 * Chamber-Master Continuous Rotation Servo Tuning Sketch
 * 
 * For continuous rotation servos (e.g., FS90R, SM-S4303R, or modified SG90).
 * These servos spin continuously instead of going to fixed angles.
 * 
 * Purpose:
 * Find the exact pulse widths for:
 *   - True STOP (no movement)
 *   - Forward direction speed (closing vent)
 *   - Reverse direction speed (opening vent)
 * 
 * Instructions:
 * 1. Connect ONLY the servo:
 *    - Signal → GPIO 5
 *    - VCC (Red) → 5V (use external 5V supply if possible)
 *    - GND → GND
 * 2. Upload this sketch
 * 3. Open Serial Monitor (115200 baud, "Newline" line ending)
 * 4. Use these commands:
 * 
 *    s          → Stop the servo immediately
 *    +10        → Increase pulse by 10µs (change direction/speed)
 *    -10        → Decrease pulse by 10µs
 *    1500       → Jump directly to 1500µs (usually near stop)
 *    time 2000  → Run servo for 2000ms at current pulse, then stop
 * 
 * Typical continuous rotation values:
 *   - Stop: around 1490–1520µs (you must find the exact value!)
 *   - Full speed one direction: ~1300µs
 *   - Full speed opposite: ~1700µs
 * 
 * Once tuned, update your main code with:
 *   SERVO_STOP_PULSE
 *   SERVO_FORWARD_PULSE (closing)
 *   SERVO_REVERSE_PULSE (opening)
 *   And the movement times (how long to run for full/half open)
 */

#include <ESP32Servo.h>

#define SERVO_PIN 5

Servo ventServo;
int currentPulse = 1500;  // Start at typical neutral

void setup() {
  Serial.begin(115200);
  ventServo.attach(SERVO_PIN, 1000, 2000);
  ventServo.writeMicroseconds(currentPulse);
  
  Serial.println("=== Continuous Rotation Servo Tuning ===");
  Serial.println("Current pulse: 1500 µs");
  Serial.println();
  Serial.println("Commands:");
  Serial.println("  s          → Stop immediately");
  Serial.println("  +10 / -10  → Adjust pulse by 10µs");
  Serial.println("  1400       → Set exact pulse (e.g., 1400)");
  Serial.println("  time 3000  → Run current pulse for 3000ms then stop");
  Serial.println();
  Serial.println("Find the pulse where servo COMPLETELY STOPS.");
  Serial.println("Then test + and - to determine open/close directions.");
  Serial.println();
}

void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    input.toLowerCase();

    if (input == "s") {
      currentPulse = 1500;  // Safe default
      ventServo.writeMicroseconds(currentPulse);
      Serial.println(">>> STOPPED (1500 µs)");
    }
    else if (input.startsWith("time ")) {
      String timeStr = input.substring(5);
      long runTime = timeStr.toInt();
      if (runTime > 0 && runTime < 20000) {  // Safety limit
        Serial.print(">>> Running at ");
        Serial.print(currentPulse);
        Serial.print(" µs for ");
        Serial.print(runTime);
        Serial.println(" ms...");
        
        ventServo.writeMicroseconds(currentPulse);
        delay(runTime);
        ventServo.writeMicroseconds(1500);  // Safe stop
        Serial.println(">>> Stopped after timed run");
      } else {
        Serial.println("Invalid time (use 100–20000 ms)");
      }
    }
    else if (input.startsWith("+") || input.startsWith("-")) {
      int adjust = input.toInt();
      if (adjust != 0) {
        currentPulse += adjust;
        currentPulse = constrain(currentPulse, 1000, 2000);
        ventServo.writeMicroseconds(currentPulse);
        
        Serial.print(">>> Pulse: ");
        Serial.print(currentPulse);
        Serial.print(" µs");
        if (currentPulse < 1480) Serial.println("  ← Likely REVERSE (opening)");
        else if (currentPulse > 1520) Serial.println("  ← Likely FORWARD (closing)");
        else if (abs(currentPulse - 1500) < 20) Serial.println("  ← Near STOP");
        Serial.println();
      }
    }
    else {
      int direct = input.toInt();
      if (direct >= 1000 && direct <= 2000) {
        currentPulse = direct;
        ventServo.writeMicroseconds(currentPulse);
        
        Serial.print(">>> Pulse set to: ");
        Serial.print(currentPulse);
        Serial.print(" µs");
        if (currentPulse < 1480) Serial.println("  ← Likely REVERSE");
        else if (currentPulse > 1520) Serial.println("  ← Likely FORWARD");
        else Serial.println("  ← Near STOP");
        Serial.println();
      } else if (direct != 0) {
        Serial.println("Pulse must be 1000–2000 µs");
      }
    }
  }
}
