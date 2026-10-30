#pragma once

#include "Arduino.h"

inline unsigned long &lastWatchdogReset() {
  static unsigned long last = 0;
  return last;
}

inline void esp_task_wdt_reset() { lastWatchdogReset() = millis(); }
