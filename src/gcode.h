#pragma once
#include <Arduino.h>

enum GCodeState { GCODE_IDLE, GCODE_RUNNING, GCODE_PAUSED };

class GCodeParser {
public:
  void init();
  void update();

  // Parse and execute a single G-code line
  void executeLine(const String &line);

  // Load a file from LittleFS and execute it
  void runFile(const String &filename);

  // Control
  void pause();
  void resume();
  void stop();

  GCodeState getState() const { return _state; }
  int getCurrentLine() const { return _currentLine; }
  int getTotalLines() const { return _totalLines; }
  float getProgress() const;
  const String &getFilename() const { return _filename; }
  void setSpeedScale(float scale);
  float getSpeedScale() const { return _speedScale; }

  void queueCommand(const String &cmd);

private:
  float _speedScale = 1.0f;
  GCodeState _state = GCODE_IDLE;
  String _filename;
  String _fileContent;
  int _currentLine = 0;
  int _totalLines = 0;
  int _parseIndex = 0;

  // Progress by bytes
  size_t _totalBytes = 0;
  size_t _processedBytes = 0;

  // Work Offset (Draw-From-Here)
  float _workOffsetX = 0.0f;
  float _workOffsetY = 0.0f;

  // Command Queue
  String _pendingCommand;
  bool _hasPendingCommand = false;

  bool _absoluteMode = true; // G90
  float _feedRate = 1000.0f; // mm/min

  void _parseLine(const String &line);
  float _parseValue(const String &line, char code, float defaultVal);
  bool _hasCode(const String &line, char code);
  void _processArc(const String &line, bool cw);
};

extern GCodeParser gcode;
