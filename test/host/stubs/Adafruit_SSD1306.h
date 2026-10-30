#pragma once

#include "Arduino.h"

constexpr int SSD1306_WHITE = 1;
constexpr int SSD1306_BLACK = 0;

class Adafruit_SSD1306 {
public:
  int width() const { return 128; }
  int height() const { return 32; }
  void fillRoundRect(int, int, int, int, int, int) {}
  void fillTriangle(int, int, int, int, int, int, int) {}
  void clearDisplay() {}
  void display() { ++flushes; }
  unsigned int flushes = 0;
};
