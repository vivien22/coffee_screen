// StartButton.cpp: see StartButton.h

#include "StartButton.h"
#include "../Config.h"

void StartButton::begin() {
  pinMode(pin_, INPUT_PULLUP);
  raw_ = down_ = (digitalRead(pin_) == LOW);   // a button that is held at power-up does nothing
  longGiven_ = true;                           // (nor its long push)
}

ButtonEvent StartButton::update(uint32_t now) {
  const bool pushed = (digitalRead(pin_) == LOW);
  if (pushed != raw_) {
    raw_ = pushed;
    changedAt_ = now;
  }
  if (raw_ != down_ && (uint32_t)(now - changedAt_) >= START_BUTTON_DEBOUNCE_MS) {
    down_ = raw_;
    if (down_) {
      downAt_ = now;
      longGiven_ = false;
      return BUTTON_PUSH;
    }
  }
  if (down_ && !longGiven_ && (uint32_t)(now - downAt_) >= LONG_PUSH_MS) {
    longGiven_ = true;
    return BUTTON_LONG;
  }
  return BUTTON_NONE;
}
