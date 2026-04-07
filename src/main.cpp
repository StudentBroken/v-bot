#include "calibration.h"
#include "config.h"
#include "gcode.h"
#include "log_buffer.h"
#include "motion.h"
#include "servo_pen.h"
#include "settings.h"
#include "status_led.h"
#include "webserver.h"
#include <Arduino.h>
#include <LittleFS.h>

PenServo penServo;
StatusLED statusLED;

// Direct pin-toggle motor test (bypasses AccelStepper)
void _testMotor(int stepPin, int dirPin, int steps) {
  // Forward
  digitalWrite(dirPin, HIGH);
  Serial.println("  → Direction: HIGH (forward)");
  for (int i = 0; i < steps; i++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(800);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(800);
  }
  delay(500);

  // Backward
  digitalWrite(dirPin, LOW);
  Serial.println("  → Direction: LOW (backward)");
  for (int i = 0; i < steps; i++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(800);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(800);
  }
  Serial.println("[Test] Done. Motor should have returned to start.");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n========================================");
  Serial.println("  V-Bot Wall Plotter Firmware v2.0 (RTOS)");
  Serial.println("========================================");

  // Initialize filesystem
  Serial.printf("[Boot] %lums: LittleFS...\n", millis());
  if (!LittleFS.begin(true)) {
    Serial.println("[FS] LittleFS mount failed!");
  } else {
    Serial.println("[FS] LittleFS mounted");
  }

  // Load settings first (WiFi creds needed)
  Serial.printf("[Boot] %lums: Settings...\n", millis());
  settings.load();

  // SANITY CHECK: If settings are garbage (e.g. 0), reset them
  if (settings.steps_per_mm < 1.0f || settings.anchor_width_mm < 100.0f) {
    Serial.println("[Boot] Settings appear invalid! Resetting to Defaults...");
    // Delete file to force default reload
    LittleFS.remove("/settings.json");
    settings = Settings(); // Reset struct
    settings.save();
  }

  Serial.printf("[Boot] Config: Width=%.1f Steps/mm=%.1f MaxSpeed=%.1f\n",
                settings.anchor_width_mm, settings.steps_per_mm,
                settings.max_speed_mm_min);

  // Start WiFi AP early so it's visible ASAP
  Serial.printf("[Boot] %lums: WiFi AP...\n", millis());
  setupWiFiAP();

  // Initialize motion (Starts FreeRTOS Task)
  Serial.printf("[Boot] %lums: Motion...\n", millis());
  motion.init();

  // Initialize servo (before LED to avoid RMT channel conflicts)
  Serial.printf("[Boot] %lums: Servo...\n", millis());
  penServo.init();

  Serial.printf("[Boot] %lums: GCode...\n", millis());
  gcode.init();
  calibration.init();

  // LED init after servo to avoid ESP32-S3 RMT/LEDC conflict
  Serial.printf("[Boot] %lums: LED...\n", millis());
  statusLED.init();

  // Start web server
  Serial.printf("[Boot] %lums: WebServer...\n", millis());
  setupWebServer();

  // Force calibration on boot
  calibration.start();

  statusLED.set(LED_READY);
  Serial.printf("[Boot] %lums: READY!\n", millis());
  Serial.println("[Ready] Connect to WiFi and open http://192.168.4.1");
  Serial.println("[Ready] Or send G-code via Serial\n");
}

void loop() {
  // 1. Update Motion persistence (Low priority)
  motion.update();

  // 2. G-Code Parser
  // This might block if the motion queue is full, effectively throttling
  // the parser to match the machine speed. This is desired.
  gcode.update();

  // 3. Calibration Logic
  calibration.update();

  // 4. Update LED based on state
  if (calibration.getStep() != CAL_IDLE &&
      calibration.getStep() != CAL_COMPLETE) {
    statusLED.set(LED_CALIBRATING);
  } else if (gcode.getState() == GCODE_PAUSED) {
    statusLED.set(LED_PAUSED);
  } else if (gcode.getState() == GCODE_RUNNING) {
    statusLED.set(LED_DRAWING);
  } else if (motion.isBusy()) {
    statusLED.set(LED_MOVING);
  } else {
    statusLED.set(LED_READY);
  }
  statusLED.update();

  // 5. Serial Input
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
      return;

    Serial.printf("> %s\n", line.c_str());

    // ── Test Commands ──
    if (line.equalsIgnoreCase("TEST")) {
      Serial.println("\n=== V-Bot Test Commands ===");
      Serial.println(
          "TEST_LEFT   - Spin left motor 500 steps forward, then back");
      Serial.println(
          "TEST_RIGHT  - Spin right motor 500 steps forward, then back");
      Serial.println("TEST_SERVO  - Toggle pen up/down");
      Serial.println("TEST_RELEASE - Release 50mm on both motors");
      Serial.println("TEST_RETRACT - Retract 50mm on both motors");
      Serial.println("STATUS      - Show current position & cable lengths");
      Serial.println(
          "Any G-code   - Execute directly (G0, G1, G28, M3, M5, etc)\n");

    } else if (line.equalsIgnoreCase("TEST_LEFT")) {
      Serial.println("[Test] Left motor: 500 steps FORWARD then BACK");
      _testMotor(X_STEP_PIN, X_DIR_PIN, 500);

    } else if (line.equalsIgnoreCase("TEST_RIGHT")) {
      Serial.println("[Test] Right motor: 500 steps FORWARD then BACK");
      _testMotor(Y_STEP_PIN, Y_DIR_PIN, 500);

    } else if (line.equalsIgnoreCase("TEST_SERVO")) {
      if (penServo.isDown()) {
        penServo.up();
      } else {
        penServo.down();
      }

    } else if (line.equalsIgnoreCase("TEST_RELEASE")) {
      Serial.println("[Test] Releasing 50mm on both motors...");
      motion.releaseString(50);
      Serial.println("[Test] Done.");

    } else if (line.equalsIgnoreCase("TEST_RETRACT")) {
      Serial.println("[Test] Retracting 50mm on both motors...");
      motion.releaseString(-50);
      Serial.println("[Test] Done.");

    } else if (line.equalsIgnoreCase("STATUS")) {
      Serial.printf("[Status] Pen: (%.1f, %.1f) mm\n", motion.getLivePenX(),
                    motion.getLivePenY());
      Serial.printf("[Status] Cables: L=%.1f R=%.1f mm\n",
                    motion.getLeftCableLength(), motion.getRightCableLength());
      Serial.printf("[Status] Pen %s\n", penServo.isDown() ? "DOWN" : "UP");
      Serial.printf("[Status] GCode state: %d\n", (int)gcode.getState());
      Serial.printf("[Status] Memory: %d bytes\n", ESP.getFreeHeap());

    } else {
      // Treat as G-code
      gcode.executeLine(line);
    }
  }
}
