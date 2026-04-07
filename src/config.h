#pragma once

// ── Pin Assignments (ESP32-S3 SuperMini) ──
#define X_STEP_PIN 1 // Left motor STEP
#define X_DIR_PIN 2  // Left motor DIR
#define Y_STEP_PIN 4 // Right motor STEP
#define Y_DIR_PIN 5  // Right motor DIR
#define SERVO_PIN 7  // SG90 pen lift
#define ENABLE_PIN 6 // A4988 enable (LOW = enabled)

// ── Stepper Constants ──
#define MICROSTEPS 16
#define STEPS_PER_REV 200
#define FULL_STEPS_PER_REV (STEPS_PER_REV * MICROSTEPS) // 3200

// ── Defaults ──
#define DEFAULT_ANCHOR_WIDTH 1300.0f // mm between anchor points
#define DEFAULT_GONDOLA_WIDTH                                                  \
  0.0f // mm between string attachment points on gondola
#define DEFAULT_STEPS_PER_MM 50.93f // (200 * 16) / (20 * PI) = ~50.929
#define DEFAULT_MAX_SPEED 8000.0f   // mm/min
#define DEFAULT_ACCELERATION 200.0f // mm/s²
#define DEFAULT_PEN_UP_ANGLE 60
#define DEFAULT_PEN_DOWN_ANGLE 150
#define DEFAULT_PEN_OFFSET_Y 10.0f          // mm, pen below string attach point
#define DEFAULT_SEGMENT_LENGTH 1.0f         // mm interpolation segments
#define DEFAULT_DRAW_WIDTH 1000.0f          // mm
#define DEFAULT_DRAW_HEIGHT 1000.0f         // mm
#define DEFAULT_CALIBRATION_RELEASE 1000.0f // mm of string to release
#define DEFAULT_INVERT_LEFT true
#define DEFAULT_INVERT_RIGHT false // Flipped based on user report
#define DEFAULT_WIFI_SSID "V-Bot"
#define DEFAULT_WIFI_PASSWORD "vbot1234"

// ── Motion ──
#define STEP_PULSE_US 4

// ── Task Configuration ──
#define MOT_TASK_CORE 1  // Run motion on Core 1 (App Core)
#define MOT_TASK_PRIO 20 // High priority
#define MOT_BUF_SIZE 64  // Non-blocking command buffer size
