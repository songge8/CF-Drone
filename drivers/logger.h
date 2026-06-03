#pragma once

#include <Arduino.h>

class Logger {
public:
  void log(const char *format, ...) {
    if (!verbose) return;

    // construct message
    char msg[100];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    if (logName != nullptr) {
      print(logName);
      print(": ");
    }
    print(msg);
    print("\n");
  }

  inline void setLogOutput(Print& output) { logOutput = &output; }
  inline void setVerbosity(const bool _verbose) { verbose = _verbose; }

protected:
  inline void setLogName(const char* name) { logName = name; }

private:
  const char* logName = nullptr;
  Print *logOutput = nullptr;
  bool verbose = true;

  void print(const char *msg) {
    if (logOutput != nullptr) {
      logOutput->print(msg);
    } else {
      #ifdef ESP32
        log_printf("%s", msg);
      #else
        // default to Serial
        Serial.print(msg);
      #endif
    }
  }
};
