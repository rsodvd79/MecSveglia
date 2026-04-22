#include "eyes.h"

#include <Arduino.h>

classEye::classEye(Adafruit_SSD1306 *disp) : display(disp) {
  resetEyes(false);
  nextUpdateMs = millis();
}

void classEye::drawEyes() {
  int r_left = safeRadius(corner_radius, left_eye.width, left_eye.height);
  int x_left = left_eye.x - left_eye.width / 2;
  int y_left = left_eye.y - left_eye.height / 2;
  display->fillRoundRect(x_left, y_left, left_eye.width, left_eye.height,
                         r_left, SSD1306_WHITE);

  int r_right = safeRadius(corner_radius, right_eye.width, right_eye.height);
  int x_right = right_eye.x - right_eye.width / 2;
  int y_right = right_eye.y - right_eye.height / 2;
  display->fillRoundRect(x_right, y_right, right_eye.width, right_eye.height,
                         r_right, SSD1306_WHITE);
}

void classEye::drawFrame() {
  display->clearDisplay();
  drawEyes();
  display->display();
}

void classEye::resetEyes(bool update) {
  const int screenW = display->width();
  const int screenH = display->height();

  int eyeHeight = min(REF_EYE_HEIGHT, screenH - 4);
  int eyeWidth = min(REF_EYE_WIDTH, (screenW / 2) - REF_SPACE_BETWEEN_EYE - 2);

  left_eye.height = eyeHeight;
  left_eye.width = eyeWidth;
  right_eye.height = eyeHeight;
  right_eye.width = eyeWidth;

  left_eye.x = screenW / 2 - eyeWidth / 2 - REF_SPACE_BETWEEN_EYE / 2;
  left_eye.y = screenH / 2;
  right_eye.x = screenW / 2 + eyeWidth / 2 + REF_SPACE_BETWEEN_EYE / 2;
  right_eye.y = screenH / 2;

  corner_radius = REF_CORNER_RADIUS;

  if (update) {
    drawFrame();
  }
}

void classEye::launchAnimation(Animation animation) {
  currentAnimation = animation;
  step = 0;
  subStep = 0;
  saccadeCount = 0;
  animationRunning = true;
  nextUpdateMs = millis();
}

void classEye::finishAnimation(unsigned long now, unsigned long pauseMs) {
  animationRunning = false;
  nextUpdateMs = now + pauseMs;
}

void classEye::scheduleNext(unsigned long now, unsigned long deltaMs) {
  nextUpdateMs = now + deltaMs;
}

void classEye::blinkStep(bool longPause, unsigned long now) {
  if (step == 0) {
    resetEyes(false);
  }
  if (step < 3) {
    left_eye.height -= 6;
    right_eye.height -= 6;
    int current_h = left_eye.height;
    int mapped_radius = map(current_h, 4, REF_EYE_HEIGHT, 1, REF_CORNER_RADIUS);
    corner_radius = min(mapped_radius, max(1, current_h / 2));
    left_eye.width += 2;
    right_eye.width += 2;
    drawFrame();
    scheduleNext(now, 25);
    step++;
    return;
  }
  if (step < 6) {
    left_eye.height += 6;
    right_eye.height += 6;
    int current_h = left_eye.height;
    int mapped_radius = map(current_h, 4, REF_EYE_HEIGHT, 1, REF_CORNER_RADIUS);
    corner_radius = min(mapped_radius, max(1, current_h / 2));
    left_eye.width -= 2;
    right_eye.width -= 2;
    drawFrame();
    scheduleNext(now, 25);
    step++;
    return;
  }
  resetEyes(true);
  finishAnimation(now, longPause ? 220 : 140);
}

void classEye::wakeupStep(unsigned long now) {
  if (step == 0) {
    resetEyes(false);
    left_eye.height = right_eye.height = 2;
    corner_radius = 1;
    drawFrame();
    scheduleNext(now, 25);
    step = 1;
    return;
  }
  if (left_eye.height < REF_EYE_HEIGHT) {
    left_eye.height += 2;
    right_eye.height += 2;
    int mapped_radius = map(left_eye.height, 2, REF_EYE_HEIGHT, 1, REF_CORNER_RADIUS);
    corner_radius = min(mapped_radius, left_eye.height / 2);
    drawFrame();
    scheduleNext(now, 25);
    return;
  }
  resetEyes(true);
  finishAnimation(now, 320);
}

void classEye::happyStep(unsigned long now) {
  if (step == 0) {
    resetEyes(true);
    happyOffset = REF_EYE_HEIGHT / 2;
    step = 1;
    scheduleNext(now, 40);
    return;
  }
  if (happyOffset >= -REF_EYE_HEIGHT / 2) {
    display->fillTriangle(
        left_eye.x - left_eye.width / 2 - 1, left_eye.y + happyOffset,
        left_eye.x + left_eye.width / 2 + 1, left_eye.y + 5 + happyOffset,
        left_eye.x - left_eye.width / 2 - 1, left_eye.y + left_eye.height + happyOffset,
        SSD1306_BLACK);
    display->fillTriangle(
        right_eye.x + right_eye.width / 2 + 1, right_eye.y + happyOffset,
        right_eye.x - right_eye.width / 2 - 2, right_eye.y + 5 + happyOffset,
        right_eye.x + right_eye.width / 2 + 1,
        right_eye.y + right_eye.height + happyOffset, SSD1306_BLACK);
    display->display();
    happyOffset -= 2;
    scheduleNext(now, 40);
    return;
  }
  resetEyes(true);
  finishAnimation(now, 500);
}

void classEye::saccadeStep(unsigned long now) {
  if (step == 0) {
    resetEyes(true);
    step = 1;
    saccadeCount = 0;
    subStep = 0;
    scheduleNext(now, 35);
    return;
  }
  if (saccadeCount >= 8) {
    resetEyes(true);
    finishAnimation(now, 200);
    return;
  }
  if (subStep == 0) {
    dirX = random(-1, 2);
    dirY = random(-1, 2);
    left_eye.x += 6 * dirX;
    right_eye.x += 6 * dirX;
    left_eye.y += 4 * dirY;
    right_eye.y += 4 * dirY;
    left_eye.height -= 3;
    right_eye.height -= 3;
    drawFrame();
    subStep = 1;
    scheduleNext(now, 30);
    return;
  }
  left_eye.x -= 6 * dirX;
  right_eye.x -= 6 * dirX;
  left_eye.y -= 4 * dirY;
  right_eye.y -= 4 * dirY;
  left_eye.height += 3;
  right_eye.height += 3;
  drawFrame();
  subStep = 0;
  saccadeCount++;
  scheduleNext(now, 30);
}

void classEye::sleepEyes() {
  resetEyes(false);
  left_eye.height = 2;
  right_eye.height = 2;
  corner_radius = 0;
  drawFrame();
}

void classEye::moveBigEyeStep(int direction, unsigned long now) {
  const int OVERSIZE_AMOUNT = 1;
  const int MOVEMENT_AMPLITUDE = 2;
  const int BLINK_AMPLITUDE = 4;
  bool returning = step >= 6;
  int localStep = returning ? (step - 6) : step;
  int sign = returning ? -1 : 1;

  left_eye.x += sign * MOVEMENT_AMPLITUDE * direction;
  right_eye.x += sign * MOVEMENT_AMPLITUDE * direction;
  right_eye.height += sign * ((localStep < 3) ? -BLINK_AMPLITUDE : BLINK_AMPLITUDE);
  left_eye.height += sign * ((localStep < 3) ? -BLINK_AMPLITUDE : BLINK_AMPLITUDE);

  EyeState *target_eye = (direction > 0) ? &right_eye : &left_eye;
  target_eye->height += sign * OVERSIZE_AMOUNT;
  target_eye->width += sign * OVERSIZE_AMOUNT;

  drawFrame();
  step++;
  if (step >= 12) {
    resetEyes(true);
    finishAnimation(now, 180);
    return;
  }
  scheduleNext(now, 30);
}

bool classEye::stepAnimation(unsigned long now) {
  switch (currentAnimation) {
  case WAKEUP:
    wakeupStep(now);
    return animationRunning;
  case RESET:
    resetEyes(true);
    finishAnimation(now, 280);
    return false;
  case MOVE_RIGHT_BIG:
    moveBigEyeStep(1, now);
    return animationRunning;
  case MOVE_LEFT_BIG:
    moveBigEyeStep(-1, now);
    return animationRunning;
  case BLINK_LONG:
    blinkStep(true, now);
    return animationRunning;
  case BLINK_SHORT:
    blinkStep(false, now);
    return animationRunning;
  case HAPPY:
    happyStep(now);
    return animationRunning;
  case SLEEP:
    sleepEyes();
    finishAnimation(now, 600);
    return false;
  case SACCADE_RANDOM:
    saccadeStep(now);
    return animationRunning;
  default:
    animationRunning = false;
    return false;
  }
}

void classEye::loop() {
  unsigned long now = millis();
  if ((long)(now - nextUpdateMs) < 0) {
    return;
  }

  if (!animationRunning) {
    if (demo_mode == 1) {
      launchAnimation(static_cast<Animation>(current_animation_index));
      current_animation_index = (current_animation_index + 1) % MAX_ANIMATIONS;
    } else {
      return;
    }
  }

  stepAnimation(now);
}

int classEye::safeRadius(int r, int w, int h) const {
  if (w < 2 * (r + 1)) {
    r = (w / 2) - 1;
  }
  if (h < 2 * (r + 1)) {
    r = (h / 2) - 1;
  }
  return (r < 0) ? 0 : r;
}
