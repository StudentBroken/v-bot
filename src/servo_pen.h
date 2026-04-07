#pragma once
#include "config.h"
#include "settings.h"
#include <Arduino.h>

// Direct LEDC PWM servo control — avoids ESP32Servo library hang on S3
// Using channel 4 and 14-bit resolution (safe for 50Hz)
#define SERVO_LEDC_CHANNEL 4
#define SERVO_LEDC_FREQ 50
#define SERVO_LEDC_BITS 14 // 16384 total steps

class PenServo {
public:
  void init() {
    // Try to clear any existing mapping
    ledcDetachPin(SERVO_PIN);

    ledcSetup(SERVO_LEDC_CHANNEL, SERVO_LEDC_FREQ, SERVO_LEDC_BITS);
    ledcAttachPin(SERVO_PIN, SERVO_LEDC_CHANNEL);

    up();
    Serial.printf("[Servo] Init: Channel %d, Pin %d\n", SERVO_LEDC_CHANNEL,
                  SERVO_PIN);
  }

  void up() {
    _writeAngle(settings.pen_up_angle);
    _penDown = false;
    Serial.printf("[Servo] UP (%d deg)\n", settings.pen_up_angle);
  }

  void down() {
    _writeAngle(settings.pen_down_angle);
    _penDown = true;
    Serial.printf("[Servo] DOWN (%d deg)\n", settings.pen_down_angle);
  }

  bool isDown() const { return _penDown; }

private:
  bool _penDown = false;

  void _writeAngle(int angle) {
    // Convert angle (0-180) to pulse width (500-2400us)
    uint16_t pulseUs = map(angle, 0, 180, 500, 2400);

    // 50Hz = 20000us period, 14-bit = 16384 steps
    // Resolution factor = 16384 / 20000 = 0.8192
    uint32_t duty = (uint32_t)pulseUs * 16384 / 20000;

    ledcWrite(SERVO_LEDC_CHANNEL, duty);
  }
};

extern PenServo penServo;
