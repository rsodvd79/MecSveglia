#pragma once

#include <stdint.h>

class RefreshSchedule {
public:
  explicit RefreshSchedule(uint32_t interval) : interval_(interval) {}

  bool due(uint32_t now, bool forced = false) const {
    return forced || !attempted_ || now - lastAttempt_ >= wait_;
  }

  void completed(uint32_t now, bool success) {
    attempted_ = true;
    lastAttempt_ = now;
    wait_ = success ? interval_ : 60000UL;
  }

private:
  uint32_t interval_;
  uint32_t lastAttempt_ = 0;
  uint32_t wait_ = 0;
  bool attempted_ = false;
};
