#include "settings.h"
#include <LittleFS.h>

Settings settings;

static const char *SETTINGS_FILE = "/settings.json";

void Settings::toJson(JsonDocument &doc) {
  doc["anchor_width_mm"] = anchor_width_mm;
  doc["gondola_width_mm"] = gondola_width_mm;
  doc["steps_per_mm"] = steps_per_mm;
  doc["max_speed_mm_min"] = max_speed_mm_min;
  doc["acceleration"] = acceleration;
  doc["pen_up_angle"] = pen_up_angle;
  doc["pen_down_angle"] = pen_down_angle;
  doc["pen_offset_y_mm"] = pen_offset_y_mm;
  doc["segment_length_mm"] = segment_length_mm;
  doc["draw_width_mm"] = draw_width_mm;
  doc["draw_height_mm"] = draw_height_mm;
  doc["calibration_release_mm"] = calibration_release_mm;
  doc["invert_left_dir"] = invert_left_dir;
  doc["invert_right_dir"] = invert_right_dir;
  doc["wifi_ssid"] = wifi_ssid;
  doc["wifi_password"] = wifi_password;

  // Persistence
  doc["last_pen_x"] = last_pen_x;
  doc["last_pen_y"] = last_pen_y;
  doc["last_left_cable"] = last_left_cable;
  doc["last_right_cable"] = last_right_cable;
}

void Settings::fromJson(JsonDocument &doc) {
  anchor_width_mm = doc["anchor_width_mm"] | anchor_width_mm;
  gondola_width_mm = doc["gondola_width_mm"] | gondola_width_mm;
  steps_per_mm = doc["steps_per_mm"] | steps_per_mm;
  max_speed_mm_min = doc["max_speed_mm_min"] | max_speed_mm_min;
  acceleration = doc["acceleration"] | acceleration;
  pen_up_angle = doc["pen_up_angle"] | pen_up_angle;
  pen_down_angle = doc["pen_down_angle"] | pen_down_angle;
  pen_offset_y_mm = doc["pen_offset_y_mm"] | pen_offset_y_mm;
  segment_length_mm = doc["segment_length_mm"] | segment_length_mm;
  draw_width_mm = doc["draw_width_mm"] | draw_width_mm;
  draw_height_mm = doc["draw_height_mm"] | draw_height_mm;
  calibration_release_mm =
      doc["calibration_release_mm"] | calibration_release_mm;

  if (doc.containsKey("invert_left_dir"))
    invert_left_dir = doc["invert_left_dir"];
  if (doc.containsKey("invert_right_dir"))
    invert_right_dir = doc["invert_right_dir"];

  if (doc["wifi_ssid"].is<const char *>())
    strlcpy(wifi_ssid, doc["wifi_ssid"], sizeof(wifi_ssid));
  if (doc["wifi_password"].is<const char *>())
    strlcpy(wifi_password, doc["wifi_password"], sizeof(wifi_password));

  // Persistence
  last_pen_x = doc["last_pen_x"] | last_pen_x;
  last_pen_y = doc["last_pen_y"] | last_pen_y;
  last_left_cable = doc["last_left_cable"] | last_left_cable;
  last_right_cable = doc["last_right_cable"] | last_right_cable;
}

void Settings::load() {
  if (!LittleFS.exists(SETTINGS_FILE)) {
    Serial.println("[Settings] No settings file, using defaults");
    save(); // Create with defaults
    return;
  }
  File f = LittleFS.open(SETTINGS_FILE, "r");
  if (!f) {
    Serial.println("[Settings] Failed to open settings file");
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("[Settings] JSON parse error: %s\n", err.c_str());
    return;
  }
  fromJson(doc);
  Serial.println("[Settings] Loaded from flash");
}

#include "status_led.h"

void Settings::save() {
  statusLED.set(LED_SAVING);
  statusLED.update(); // Force immediate update

  File f = LittleFS.open(SETTINGS_FILE, "w");
  if (!f) {
    Serial.println("[Settings] Failed to open file for writing");
    statusLED.set(LED_ERROR);
    return;
  }
  JsonDocument doc;
  toJson(doc);
  if (serializeJsonPretty(doc, f) == 0) {
    Serial.println("[Settings] Failed to write JSON to file");
    statusLED.set(LED_ERROR);
  } else {
    Serial.println("[Settings] Saved to flash");
  }
  f.close();

  // Restore previous state (approximate)
  delay(100);
  statusLED.set(LED_READY);
}
