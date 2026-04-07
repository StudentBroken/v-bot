#include "webserver.h"
#include "calibration.h"
#include "gcode.h"
#include "log_buffer.h"
#include "motion.h"
#include "servo_pen.h"
#include "settings.h"
#include "status_led.h"
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <LittleFS.h>
#include <WiFi.h>

static AsyncWebServer server(80);

void setupWiFiAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(settings.wifi_ssid, settings.wifi_password);
  LOG("[WiFi] AP started: %s @ %s", settings.wifi_ssid,
      WiFi.softAPIP().toString().c_str());
}

// ── Helper: JSON response ──
static void sendJson(AsyncWebServerRequest *req, int code, JsonDocument &doc) {
  String json;
  serializeJson(doc, json);
  req->send(code, "application/json", json);
}

void setupWebServer() {
  // ── Serve WebUI ──
  // ── Serve WebUI ──
  // Serve static files from root of LittleFS
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // ── GET /api/status ──
  // ── GET /api/status ──
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req) {
    JsonDocument doc;
    // CRITICAL: use LIVE position from steps, not cached target
    doc["x"] = motion.getLivePenX();
    doc["y"] = motion.getLivePenY();

    // Convert steps to mm for cable lengths to be accurate during moves
    float lSteps = (float)motion.getLeftSteps();
    float rSteps = (float)motion.getRightSteps();
    doc["leftCable"] = lSteps / settings.steps_per_mm;
    doc["rightCable"] = rSteps / settings.steps_per_mm;

    doc["penDown"] = penServo.isDown();
    doc["busy"] = motion.isBusy();
    doc["gcodeState"] = (int)gcode.getState();
    doc["gcodeFile"] = gcode.getFilename();
    doc["gcodeLine"] = gcode.getCurrentLine();
    doc["gcodeTotal"] = gcode.getTotalLines();
    doc["gcodeProgress"] = gcode.getProgress();
    doc["calStep"] = (int)calibration.getStep();
    doc["calDesc"] = calibration.getStepDescription();
    doc["motorsEnabled"] = motion.areMotorsEnabled();
    doc["speedScale"] = gcode.getSpeedScale();

    // Debug Math (keep these)
    doc["stepsL"] = motion.getLeftSteps();
    doc["stepsR"] = motion.getRightSteps();
    doc["confWidth"] = settings.anchor_width_mm;
    doc["confStepsPerMm"] = settings.steps_per_mm;

    sendJson(req, 200, doc);
  });

  // ── POST /api/jog ──
  server.on(
      "/api/jog", HTTP_POST, [](AsyncWebServerRequest *req) {}, NULL,
      [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t,
         size_t) {
        JsonDocument doc;
        deserializeJson(doc, data, len);
        float dx = doc["dx"] | 0.0f;
        float dy = doc["dy"] | 0.0f;
        float speed = doc["speed"] | 0.0f; // Extract speed

        // Use jogCartesian for manual moves to fix direction mapping
        motion.jogCartesian(dx, dy, speed);

        JsonDocument resp;
        resp["ok"] = true;
        sendJson(req, 200, resp);
      });

  // ── POST /api/gcode — single line ──
  server.on(
      "/api/gcode", HTTP_POST, [](AsyncWebServerRequest *req) {}, NULL,
      [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t,
         size_t) {
        // Safe string creation
        String line = "";
        line.reserve(len);
        for (size_t i = 0; i < len; i++) {
          line += (char)data[i];
        }

        // Queue for main loop execution to avoid blocking WebServer (WDT reset
        // prevention)
        gcode.queueCommand(line);

        JsonDocument resp;
        resp["ok"] = true;
        sendJson(req, 200, resp);
      });

  // ── POST /api/upload — G-code file upload ──
  server.on(
      "/api/upload", HTTP_POST,
      [](AsyncWebServerRequest *req) {
        JsonDocument resp;
        resp["ok"] = true;
        sendJson(req, 200, resp);
      },
      [](AsyncWebServerRequest *req, const String &filename, size_t index,
         uint8_t *data, size_t len, bool final) {
        static File uploadFile;
        String path = "/" + filename;

        if (index == 0) {
          LOG("[Upload] Start: %s", filename.c_str());
          uploadFile = LittleFS.open(path, "w");
        }
        if (uploadFile) {
          uploadFile.write(data, len);
        }
        if (final) {
          if (uploadFile)
            uploadFile.close();
          LOG("[Upload] Complete: %s (%d bytes)", filename.c_str(),
              index + len);
        }
      });

  // ── GET /api/files — list uploaded G-code files ──
  server.on("/api/files", HTTP_GET, [](AsyncWebServerRequest *req) {
    JsonDocument doc;
    JsonArray files = doc["files"].to<JsonArray>();
    File root = LittleFS.open("/");
    File f = root.openNextFile();
    while (f) {
      String name = f.name();
      if (name.endsWith(".gcode") || name.endsWith(".gc") ||
          name.endsWith(".nc")) {
        JsonObject obj = files.add<JsonObject>();
        obj["name"] = name;
        obj["size"] = f.size();
      }
      f = root.openNextFile();
    }
    sendJson(req, 200, doc);
  });

  // ── POST /api/override ──
  server.on(
      "/api/override", HTTP_POST, [](AsyncWebServerRequest *req) {}, NULL,
      [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t,
         size_t) {
        JsonDocument doc;
        deserializeJson(doc, data, len);
        float scale = doc["scale"] | 1.0f;
        if (scale < 0.1f)
          scale = 0.1f;
        if (scale > 2.5f)
          scale = 2.5f; // Limit to 250%
        gcode.setSpeedScale(scale);
        JsonDocument resp;
        resp["ok"] = true;
        resp["scale"] = gcode.getSpeedScale();
        sendJson(req, 200, resp);
      });

  // ── POST /api/run — run a G-code file ──
  server.on(
      "/api/run", HTTP_POST, [](AsyncWebServerRequest *req) {}, NULL,
      [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t,
         size_t) {
        JsonDocument doc;
        deserializeJson(doc, data, len);
        String filename = doc["file"] | "";
        gcode.runFile(filename);
        JsonDocument resp;
        resp["ok"] = true;
        sendJson(req, 200, resp);
      });

  // ── POST /api/pause ──
  server.on("/api/pause", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (gcode.getState() == GCODE_PAUSED) {
      gcode.resume();
    } else {
      gcode.pause();
    }
    JsonDocument resp;
    resp["ok"] = true;
    sendJson(req, 200, resp);
  });

  // ── POST /api/stop ──
  server.on("/api/stop", HTTP_POST, [](AsyncWebServerRequest *req) {
    gcode.stop();
    JsonDocument resp;
    resp["ok"] = true;
    sendJson(req, 200, resp);
  });

  // ── POST /api/reboot ──
  server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *req) {
    JsonDocument resp;
    resp["ok"] = true;
    sendJson(req, 200, resp);

    // Schedule reboot
    statusLED.set(LED_SAVING); // Reuse blue fast blink for reboot
    statusLED.update();
    Serial.println("[Web] Reboot requested");
    delay(1000);
    ESP.restart();
  });

  // ── POST /api/motors — enable/disable motors ──
  server.on(
      "/api/motors", HTTP_POST, [](AsyncWebServerRequest *req) {}, NULL,
      [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t,
         size_t) {
        JsonDocument doc;
        deserializeJson(doc, data, len);
        bool enable = doc["enable"] | true;
        if (enable) {
          motion.enableMotors();
        } else {
          // Only allow disable if explicit
          motion.disableMotors();
        }
        JsonDocument resp;
        resp["ok"] = true;
        resp["motorsEnabled"] = motion.areMotorsEnabled();
        sendJson(req, 200, resp);
      });

  // ── GET /api/settings ──
  server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *req) {
    JsonDocument doc;
    settings.toJson(doc);
    sendJson(req, 200, doc);
  });

  // ── POST /api/settings ──
  server.on(
      "/api/settings", HTTP_POST, [](AsyncWebServerRequest *req) {}, NULL,
      [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t,
         size_t) {
        JsonDocument doc;
        deserializeJson(doc, data, len);
        settings.fromJson(doc);
        settings.save();
        motion.init(); // Re-apply speeds/accel
        JsonDocument resp;
        resp["ok"] = true;
        sendJson(req, 200, resp);
      });

  // ── POST /api/settings/reset ──
  server.on("/api/settings/reset", HTTP_POST, [](AsyncWebServerRequest *req) {
    // Reset to compile-time defaults
    settings = Settings();
    settings.save();
    motion.init(); // Re-apply speeds/accel
    JsonDocument resp;
    resp["ok"] = true;
    sendJson(req, 200, resp);
  });

  // ── POST /api/calibrate ──
  server.on(
      "/api/calibrate", HTTP_POST, [](AsyncWebServerRequest *req) {}, NULL,
      [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t,
         size_t) {
        JsonDocument doc;
        deserializeJson(doc, data, len);
        String action = doc["action"] | "";

        if (action == "start") {
          calibration.start();
        } else if (action == "confirm_retracted") {
          float mm = doc["release_mm"] | -1.0f;
          calibration.confirmRetracted(mm);
        } else if (action == "set_width") {
          float w = doc["width"] | 1300.0f;
          calibration.setAnchorWidth(w);
        } else if (action == "cancel") {
          calibration.cancel();
        } else if (action == "release") {
          float mm = doc["mm"] | 50.0f;
          calibration.manualRelease(mm);
        } else if (action == "retract") {
          float mm = doc["mm"] | 50.0f;
          calibration.manualRetract(mm);
        } else if (action == "set_lengths") {
          float left = doc["left"] | 1000.0f;
          float right = doc["right"] | 1000.0f;
          calibration.setLengths(left, right);
        } else if (action == "jog") {
          float left = doc["left"] | 0.0f;
          float right = doc["right"] | 0.0f;
          float speed = doc["speed"] | 0.0f;
          motion.jogMotors(left, right, speed);
        } else if (action == "stop_jog") {
          motion.stop(); // Smooth stop
        }

        JsonDocument resp;
        resp["ok"] = true;
        resp["step"] = (int)calibration.getStep();
        resp["desc"] = calibration.getStepDescription();
        resp["x"] = motion.getPenX();
        resp["y"] = motion.getPenY();
        resp["leftCable"] = motion.getLeftCableLength();
        resp["rightCable"] = motion.getRightCableLength();
        sendJson(req, 200, resp);
      });

  // ── POST /api/geometry (Unified Config) ──
  server.on(
      "/api/geometry", HTTP_POST, [](AsyncWebServerRequest *req) {}, NULL,
      [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t,
         size_t) {
        JsonDocument doc;
        deserializeJson(doc, data, len);

        // 1. Update Width Setting
        float width = doc["width"] | -1.0f;
        if (width > 0) {
          settings.anchor_width_mm = width;
          Serial.printf("[Geo] Set Width: %.1f\n", width);
        }

        // 2. Update Cable Lengths (Triangulation)
        float left = doc["left"] | -1.0f;
        float right = doc["right"] | -1.0f;

        if (left > 0 && right > 0) {
          calibration.setLengths(left, right);
          Serial.printf("[Geo] Set Lengths: L=%.1f R=%.1f\n", left, right);
        }

        // 3. Save Everything
        settings.save();

        JsonDocument resp;
        resp["ok"] = true;
        resp["x"] = motion.getPenX();
        resp["y"] = motion.getPenY();
        sendJson(req, 200, resp);
      });

  // ── POST /api/pen ──
  server.on(
      "/api/pen", HTTP_POST, [](AsyncWebServerRequest *req) {}, NULL,
      [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t,
         size_t) {
        JsonDocument doc;
        deserializeJson(doc, data, len);
        bool down = doc["down"] | false;
        if (down)
          penServo.down();
        else
          penServo.up();
        JsonDocument resp;
        resp["ok"] = true;
        resp["penDown"] = penServo.isDown();
        sendJson(req, 200, resp);
      });

  // ── POST /api/drive (Continuous Jog) ──
  server.on(
      "/api/drive", HTTP_POST, [](AsyncWebServerRequest *req) {}, NULL,
      [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t,
         size_t) {
        JsonDocument doc;
        deserializeJson(doc, data, len);
        float vx = doc["vx"] | 0.0f;
        float vy = doc["vy"] | 0.0f;

        // If vx/vy are 0, we can stop? Or just drive(0,0)?
        // Stop is safer if 0.
        if (abs(vx) < 0.01 && abs(vy) < 0.01) {
          motion.stop();
        } else {
          motion.driveXY(vx, vy);
        }

        JsonDocument resp;
        resp["ok"] = true;
        sendJson(req, 200, resp);
      });

  // ── DELETE /api/files ──
  server.on(
      "/api/files", HTTP_DELETE, [](AsyncWebServerRequest *req) {}, NULL,
      [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t,
         size_t) {
        JsonDocument doc;
        deserializeJson(doc, data, len);
        String filename = doc["file"] | "";
        String path = "/" + filename;
        if (LittleFS.exists(path)) {
          LittleFS.remove(path);
        }
        JsonDocument resp;
        resp["ok"] = true;
        sendJson(req, 200, resp);
      });

  // ── GET /api/console ──
  server.on("/api/console", HTTP_GET, [](AsyncWebServerRequest *req) {
    JsonDocument doc;
    JsonArray lines = doc["lines"].to<JsonArray>();

    if (consoleLog.hasLines()) {
      std::vector<String> logs = consoleLog.fetch();
      for (const auto &l : logs) {
        lines.add(l);
      }
    }
    sendJson(req, 200, doc);
  });

  ElegantOTA.begin(&server);
  server.begin();
  LOG("[Web] Server started on port 80");
}
