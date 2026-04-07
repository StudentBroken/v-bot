#pragma once
#include <Arduino.h>
#include <FreeRTOS.h> // Required for xSemaphoreCreateMutex, xSemaphoreTake, xSemaphoreGive
#include <semphr.h> // Required for SemaphoreHandle_t
#include <vector>

// Keep last 30 lines
#define MAX_LOG_LINES 30

class LogBuffer {
public:
  LogBuffer() { _mutex = xSemaphoreCreateMutex(); }

  void push(const String &msg) {
    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
      if (_lines.size() >= MAX_LOG_LINES) {
        _lines.erase(_lines.begin());
      }
      _lines.push_back(msg);
      xSemaphoreGive(_mutex);
    }
    // Serial is usually thread-safe enough or buffered
    // Serial.println(msg); // Removed double print
  }

  // Generic print support
  template <typename T> void println(const T &msg) { push(String(msg)); }

  void printf(const char *format, ...) {
    char loc_buf[64];
    char *temp = loc_buf;
    va_list arg;
    va_list copy;
    va_start(arg, format);
    va_copy(copy, arg);
    int len = vsnprintf(temp, sizeof(loc_buf), format, copy);
    va_end(copy);
    if (len < 0) {
      va_end(arg);
      return;
    };
    if (len >= (int)sizeof(loc_buf)) {
      temp = (char *)malloc(len + 1);
      if (temp == NULL) {
        va_end(arg);
        return;
      }
      vsnprintf(temp, len + 1, format, arg);
    }
    va_end(arg);

    // Push the formatted string
    push(String(temp));

    if (temp != loc_buf) {
      free(temp);
    }
  }

  // Fetch and clear logs (pop all)
  // Or just fetch all?
  // Use a 'dirty' index?
  // Simplest for web polling: Fetch all accumulated since last fetch.
  // We can just return the headers.

  std::vector<String> fetch() {
    std::vector<String> ret;
    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
      ret = _lines;
      _lines.clear();
      xSemaphoreGive(_mutex);
    }
    return ret;
  }

  bool hasLines() {
    bool has = false;
    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
      has = !_lines.empty();
      xSemaphoreGive(_mutex);
    }
    return has;
  }

private:
  std::vector<String> _lines;
  SemaphoreHandle_t _mutex;
};

extern LogBuffer consoleLog;

void LOG(const char *format, ...);
