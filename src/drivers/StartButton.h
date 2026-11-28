/*
  StartButton: the start / stop button (START_BUTTON in Config.h), between its pin and GND (the internal pull-up is used: no resistor).

  The level has to stay START_BUTTON_DEBOUNCE_MS to count. update() gives the push once, when it counts (the action is taken at the
  push, not at the release), and the long push once, when the button is still held LONG_PUSH_MS after the push (without waiting for
  the release). A button held at power-up does nothing until it is released. What a push does is decided by the machine.
*/

#pragma once
#include <Arduino.h>

enum ButtonEvent : uint8_t { BUTTON_NONE, BUTTON_PUSH, BUTTON_LONG };

class StartButton {
  public:
    explicit StartButton(uint8_t pin) : pin_(pin) {}

    void begin();

    // Call on every pass
    ButtonEvent update(uint32_t now);

  private:
    uint8_t pin_;
    bool raw_ = false;          // the level now (true = pushed) ...
    bool down_ = false;         // ... and the debounced one
    bool longGiven_ = false;    // the long push of this push was given
    uint32_t changedAt_ = 0;    // when the level last changed
    uint32_t downAt_ = 0;       // when the push counted
};
