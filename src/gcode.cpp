#include "gcode.h"
#include "log_buffer.h"
#include "motion.h"
#include "servo_pen.h"
#include "settings.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <cmath>

GCodeParser gcode;

void GCodeParser::init() {
  _state = GCODE_IDLE;
  _feedRate = 1000.0f; // Safe default
  consoleLog.println("[GCode] Parser ready");
}

void GCodeParser::update() {
  // 1. Handle immediate commands from WebUI — wait for motion idle first
  if (_hasPendingCommand && !motion.isBusy()) {
    String cmd = _pendingCommand;
    _hasPendingCommand = false;
    _pendingCommand = "";
    consoleLog.printf("[GCode] Executing: %s\n", cmd.c_str());
    _parseLine(cmd);
    return;
  }

  if (_state != GCODE_RUNNING)
    return;

  // 2. All lines parsed — wait for motion queue to drain, then finish
  if (_parseIndex >= (int)_fileContent.length()) {
    if (!motion.isBusy()) {
      _state = GCODE_IDLE;
      penServo.up();
      consoleLog.println("[GCode] File complete");
    }
    return;
  }

  // 3. If motion queue is saturated, don't parse yet — let it drain a bit
  if (motion.isQueueFull())
    return;

  // 4. Parse the next non-empty, non-comment line
  while (_parseIndex < (int)_fileContent.length()) {
    int nl = _fileContent.indexOf('\n', _parseIndex);
    if (nl < 0)
      nl = _fileContent.length();

    String line = _fileContent.substring(_parseIndex, nl);
    line.trim();
    _parseIndex = nl + 1;
    _processedBytes = _parseIndex;
    _currentLine++;

    if (line.length() > 0 && line[0] != ';' && line[0] != '(') {
      _parseLine(line);
      return; // one line per update() call
    }
    // Skip blank lines and comments, continue looking
  }
}

void GCodeParser::queueCommand(const String &cmd) {
  if (_hasPendingCommand) {
    consoleLog.println("[GCode] Queue full! Dropping: " + cmd);
    return;
  }
  _pendingCommand = cmd;
  _hasPendingCommand = true;
}

void GCodeParser::executeLine(const String &line) {
  String trimmed = line;
  trimmed.trim();

  // Strip comments
  int sc = trimmed.indexOf(';');
  if (sc >= 0)
    trimmed = trimmed.substring(0, sc);
  trimmed.trim();

  if (trimmed.length() > 0) {
    _parseLine(trimmed);
  }
}

void GCodeParser::setSpeedScale(float scale) {
  _speedScale = scale;
  consoleLog.printf("[GCode] Speed scale set to %.2f%%\n", scale * 100.0f);
}

void GCodeParser::_parseLine(const String &line) {
  String upper = line;
  upper.toUpperCase();

  // Strip inline comments
  int sc = upper.indexOf(';');
  if (sc >= 0)
    upper = upper.substring(0, sc);
  upper.trim();

  if (upper.length() == 0)
    return;

  if (upper.startsWith("G0 ") || upper == "G0" ||
      upper.startsWith("G1 ") || upper == "G1") {
    bool isRapid = upper[1] == '0';

    float valX = _parseValue(upper, 'X', NAN);
    float valY = _parseValue(upper, 'Y', NAN);

    float f = _parseValue(upper, 'F', NAN);
    if (!isnan(f) && f > 0) {
      _feedRate = f;
    }

    float targetX, targetY;

    if (_absoluteMode) {
      targetX = isnan(valX) ? motion.getPenX() : (valX + _workOffsetX);
      targetY = isnan(valY) ? motion.getPenY() : (valY + _workOffsetY);
    } else {
      targetX = motion.getPenX() + (isnan(valX) ? 0 : valX);
      targetY = motion.getPenY() + (isnan(valY) ? 0 : valY);
    }

    // Z axis controls pen servo
    if (_hasCode(upper, 'Z')) {
      float z = _parseValue(upper, 'Z', 0);
      bool targetDown = (z > 0.1);
      if (penServo.isDown() != targetDown) {
        if (targetDown)
          penServo.down();
        else
          penServo.up();
        delay(300);
      }
    }

    if (isRapid)
      motion.moveToRapid(targetX, targetY);
    else
      motion.moveTo(targetX, targetY, _feedRate * _speedScale);

  } else if (upper.startsWith("G2 ") || upper == "G2") {
    _processArc(upper, true); // CW

  } else if (upper.startsWith("G3 ") || upper == "G3") {
    _processArc(upper, false); // CCW

  } else if (upper.startsWith("G28")) {
    motion.moveToRapid(0, 0);

  } else if (upper.startsWith("G90")) {
    _absoluteMode = true;

  } else if (upper.startsWith("G91")) {
    _absoluteMode = false;

  } else if (upper.startsWith("G92")) {
    float x = _parseValue(upper, 'X', motion.getPenX());
    float y = _parseValue(upper, 'Y', motion.getPenY());
    motion.setPenPosition(x, y);

  } else if (upper.startsWith("M3")) {
    // Force Down
    penServo.down();
    delay(300);

  } else if (upper.startsWith("M5")) {
    // Force Up
    penServo.up();
    delay(300);

  } else if (upper.startsWith("M0")) {
    // Pause — wait for resume from WebUI
    _state = GCODE_PAUSED;
    Serial.println("[GCode] Paused (M0)");

  } else if (upper.startsWith("M112")) {
    motion.emergencyStop();
    _state = GCODE_IDLE;
  }
}

float GCodeParser::_parseValue(const String &line, char code,
                               float defaultVal) {
  int idx = line.indexOf(code);
  if (idx < 0)
    return defaultVal;

  idx++; // Skip the letter
  String numStr = "";
  while (idx < (int)line.length() &&
         (isdigit(line[idx]) || line[idx] == '.' || line[idx] == '-')) {
    numStr += line[idx];
    idx++;
  }

  if (numStr.length() == 0)
    return defaultVal;
  return numStr.toFloat();
}

bool GCodeParser::_hasCode(const String &line, char code) {
  return line.indexOf(code) >= 0;
}

void GCodeParser::runFile(const String &filename) {
  _filename = filename;
  String path = "/" + filename;

  File f = LittleFS.open(path, "r");
  if (!f) {
    Serial.printf("[GCode] Failed to open: %s\n", path.c_str());
    return;
  }

  _fileContent = f.readString();
  f.close();

  // Count lines
  _totalLines = 1;
  _totalBytes = _fileContent.length();
  _processedBytes = 0;

  // Capture Work Offset (Current Position = (0,0) for this file)
  _workOffsetX = motion.getPenX();
  _workOffsetY = motion.getPenY();
  Serial.printf("[GCode] Work Offset Set: (%.1f, %.1f)\n", _workOffsetX,
                _workOffsetY);

  for (int i = 0; i < (int)_fileContent.length(); i++) {
    if (_fileContent[i] == '\n')
      _totalLines++;
  }

  _currentLine = 0;
  _parseIndex = 0;
  _state = GCODE_RUNNING;
  Serial.printf("[GCode] Running %s (%d lines)\n", filename.c_str(),
                _totalLines);
}

void GCodeParser::pause() {
  if (_state == GCODE_RUNNING) {
    _state = GCODE_PAUSED;
    motion.pause();
  }
}

void GCodeParser::resume() {
  if (_state == GCODE_PAUSED) {
    _state = GCODE_RUNNING;
    motion.resume();
  }
}

void GCodeParser::stop() {
  _state = GCODE_IDLE;
  _fileContent = "";
  _feedRate = 1000.0f;    // Reset feedrate
  _speedScale = 1.0f;     // Reset scale
  _absoluteMode = true;   // Reset to absolute G90
  motion.emergencyStop(); // NOW valid
  penServo.up();
  Serial.println("[GCode] Stopped");
}

float GCodeParser::getProgress() const {
  if (_totalBytes <= 0)
    return 0;
  return (float)_processedBytes / _totalBytes * 100.0f;
}

void GCodeParser::_processArc(const String &line, bool cw) {
  // Feedrate
  float f = _parseValue(line, 'F', _feedRate);
  _feedRate = f;

  // Current position (Start)
  float startX = motion.getPenX();
  float startY = motion.getPenY();

  // Target position (End)
  float valX = _parseValue(line, 'X', NAN);
  float valY = _parseValue(line, 'Y', NAN);

  float endX, endY;

  if (_absoluteMode) {
    // G90: Apply Work Offset
    endX = isnan(valX) ? startX : (valX + _workOffsetX);
    endY = isnan(valY) ? startY : (valY + _workOffsetY);
  } else {
    // G91: Relative to current
    endX = startX + (isnan(valX) ? 0 : valX);
    endY = startY + (isnan(valY) ? 0 : valY);
  }

  // Offsets I, J (Relative to Start)
  // Default 0 if not present
  float i = _parseValue(line, 'I', 0);
  float j = _parseValue(line, 'J', 0);

  // Center
  float centerX = startX + i;
  float centerY = startY + j;

  // Radius
  // R = sqrt(I^2 + J^2)
  float radius = sqrtf(i * i + j * j);
  if (radius < 0.1f) {
    // Zero radius? Just linear move to end
    motion.moveTo(endX, endY, _feedRate * _speedScale);
    return;
  }

  // Angles
  // atan2(y, x) -> result in radians -PI to +PI
  float startAngle = atan2f(startY - centerY, startX - centerX);
  float endAngle = atan2f(endY - centerY, endX - centerX);

  // Handle wrap-around based on direction
  // CW: angle decreases. CCW: angle increases.

  if (cw) { // G2 - Clockwise
    if (endAngle >= startAngle) {
      endAngle -= 2 * PI;
    }
  } else { // G3 - Counter-Clockwise
    if (endAngle <= startAngle) {
      endAngle += 2 * PI;
    }
  }

  // Segment length ~1mm
  float arcLength = abs(endAngle - startAngle) * radius;
  int segments = (int)ceil(arcLength / 1.0f); // 1mm resolution
  if (segments < 1)
    segments = 1;

  float angleStep = (endAngle - startAngle) / segments;

  for (int s = 1; s <= segments; s++) {
    float angle = startAngle + (angleStep * s);
    float targetX = centerX + cosf(angle) * radius;
    float targetY = centerY + sinf(angle) * radius;
    motion.moveTo(targetX, targetY, _feedRate * _speedScale);
  }
}
