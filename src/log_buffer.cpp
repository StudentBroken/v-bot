#include "log_buffer.h"

LogBuffer consoleLog;

void LOG(const char *format, ...) {
  char loc_buf[128];
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
    len = vsnprintf(temp, len + 1, format, arg);
  }
  va_end(arg);

  // Output to Serial (blocking if buffer full)
  Serial.print(temp);

  // Output to Console Log (copy)
  consoleLog.push(String(temp));

  if (temp != loc_buf) {
    free(temp);
  }
}
