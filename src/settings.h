#pragma once
#include "config.h"
#include <Arduino.h>
#include <ArduinoJson.h>

struct Settings {
  float anchor_width_mm = DEFAULT_ANCHOR_WIDTH;
  float gondola_width_mm = DEFAULT_GONDOLA_WIDTH;
  float steps_per_mm = DEFAULT_STEPS_PER_MM;
  float max_speed_mm_min = DEFAULT_MAX_SPEED;
  float acceleration = DEFAULT_ACCELERATION;
  int pen_up_angle = DEFAULT_PEN_UP_ANGLE;
  int pen_down_angle = DEFAULT_PEN_DOWN_ANGLE;
  float pen_offset_y_mm = DEFAULT_PEN_OFFSET_Y;
  float segment_length_mm = DEFAULT_SEGMENT_LENGTH;
  float draw_width_mm = DEFAULT_DRAW_WIDTH;
  float draw_height_mm = DEFAULT_DRAW_HEIGHT;
  float calibration_release_mm = DEFAULT_CALIBRATION_RELEASE;
  bool invert_left_dir = DEFAULT_INVERT_LEFT;
  bool invert_right_dir = DEFAULT_INVERT_RIGHT;
  char wifi_ssid[32] = DEFAULT_WIFI_SSID;
  char wifi_password[64] = DEFAULT_WIFI_PASSWORD;

  // Persistence
  float last_pen_x = -1.0f;
  float last_pen_y = -1.0f;
  float last_left_cable = -1.0f;
  float last_right_cable = -1.0f;

  void load();
  void save();
  void toJson(JsonDocument &doc);
  void fromJson(JsonDocument &doc);
};

extern Settings settings;
