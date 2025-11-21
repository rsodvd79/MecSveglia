#pragma once

#include <Adafruit_SSD1306.h>

class classEye {
public:
  explicit classEye(Adafruit_SSD1306 *disp);

  // Advance eye animation without blocking the main loop.
  void loop();

private:
  enum Animation {
    WAKEUP,
    RESET,
    MOVE_RIGHT_BIG,
    MOVE_LEFT_BIG,
    BLINK_LONG,
    BLINK_SHORT,
    HAPPY,
    SLEEP,
    SACCADE_RANDOM,
    MAX_ANIMATIONS
  };

  struct EyeState {
    int height;
    int width;
    int x;
    int y;
  };

  void drawEyes();
  void drawFrame();
  void resetEyes(bool update = true);
  void blinkStep(bool longPause, unsigned long now);
  void sleepEyes();
  void wakeupStep(unsigned long now);
  void happyStep(unsigned long now);
  void saccadeStep(unsigned long now);
  void moveBigEyeStep(int direction, unsigned long now);
  void launchAnimation(Animation animation);
  bool stepAnimation(unsigned long now);
  void finishAnimation(unsigned long now, unsigned long pauseMs);
  void scheduleNext(unsigned long now, unsigned long deltaMs);
  int safeRadius(int r, int w, int h) const;

  Adafruit_SSD1306 *display;

  static constexpr int REF_EYE_HEIGHT = 16;
  static constexpr int REF_EYE_WIDTH = 32;
  static constexpr int REF_SPACE_BETWEEN_EYE = 8;
  static constexpr int REF_CORNER_RADIUS = 5;

  int demo_mode = 1;
  int current_animation_index = 0;
  EyeState left_eye{};
  EyeState right_eye{};
  int corner_radius = REF_CORNER_RADIUS;

  // Non-blocking animation state
  Animation currentAnimation = WAKEUP;
  bool animationRunning = false;
  int step = 0;
  int subStep = 0;
  int saccadeCount = 0;
  int dirX = 0;
  int dirY = 0;
  int happyOffset = 0;
  unsigned long nextUpdateMs = 0;
};
