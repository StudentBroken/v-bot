#pragma once
#include <Adafruit_NeoPixel.h>

// RGB LED on GPIO 48 (ESP32-S3 SuperMini onboard NeoPixel)
#define LED_PIN 48

enum LedStatus {
  LED_BOOT,        // White pulse — starting up
  LED_READY,       // Green breathe — idle, ready
  LED_WIFI,        // Blue solid — WiFi AP active
  LED_MOVING,      // Cyan fast pulse — motors running
  LED_DRAWING,     // Purple breathe — G-code running
  LED_PAUSED,      // Yellow blink — paused
  LED_CALIBRATING, // Orange breathe — calibration
  LED_SAVING,      // Blue fast blink — saving settings / rebooting
  LED_ERROR        // Red fast blink — error/e-stop
};

class StatusLED {
public:
  void init() {
    _pixel.begin();
    _pixel.setBrightness(25);
    set(LED_BOOT);
  }

  void set(LedStatus status) { _status = status; }

  // Call from loop() — handles animations
  void update() {
    unsigned long now = millis();
    if (now - _lastUpdate < 50)
      return; // 20 Hz refresh (Reduced from 50Hz to save CPU/interrupts)
    _lastUpdate = now;

    uint8_t r = 0, g = 0, b = 0;
    float phase = (float)(now % 2000) / 2000.0f;           // 0→1 over 2s
    float breathe = (sinf(phase * 6.2832f) + 1.0f) / 2.0f; // 0→1 sine
    bool blinkOn = (now % 500) < 250;
    bool fastBlink = (now % 200) < 100;

    switch (_status) {
    case LED_BOOT:
      r = g = b = (uint8_t)(breathe * 255);
      break;
    case LED_READY:
      g = (uint8_t)(breathe * 180 + 20);
      break;
    case LED_WIFI:
      b = 150;
      break;
    case LED_MOVING:
      g = fastBlink ? 200 : 0;
      b = fastBlink ? 200 : 0;
      break;
    case LED_DRAWING:
      r = (uint8_t)(breathe * 180);
      b = (uint8_t)(breathe * 255);
      break;
    case LED_PAUSED:
      r = blinkOn ? 255 : 0;
      g = blinkOn ? 180 : 0;
      break;
    case LED_CALIBRATING:
      r = (uint8_t)(breathe * 255);
      g = (uint8_t)(breathe * 120);
      break;
    case LED_SAVING:
      b = fastBlink ? 255 : 0;
      break;
    case LED_ERROR:
      r = fastBlink ? 255 : 0;
      break;
    }

    // Optimization: Only update strip if color changed
    uint32_t newColor = _pixel.Color(r, g, b);
    if (newColor != _lastColor) {
      _pixel.setPixelColor(0, newColor);
      _pixel.show();
      _lastColor = newColor;
    }
  }

private:
  Adafruit_NeoPixel _pixel{1, LED_PIN, NEO_GRB + NEO_KHZ800};
  LedStatus _status = LED_BOOT;
  unsigned long _lastUpdate = 0;
  uint32_t _lastColor = 0;
};

extern StatusLED statusLED;
