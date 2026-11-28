/*
  BacklightFade: the fade of the backlight at a big change of the screen (steaming mode, standby) and when the screen first comes on.

  OUT: the old screen goes dark (the new one is not drawn yet); when it is dark the caller draws the new screen, then IN: the backlight
  comes up. The fade is linear in the level (setBacklight() applies the perceptual curve). The out phase lasts in proportion to the level
  it starts from (a change during a fade in does not wait a full fade out). The start of a phase is taken by the first step() of that
  phase: the drawing of the new screen takes a few hundred ms, which must not eat the fade in.
*/

#pragma once
#include <stdint.h>

class BacklightFade {
  public:
    enum Phase : uint8_t { STEADY, OUT, IN };

    Phase phase() const { return (Phase)phase_; }
    uint8_t level() const { return level_; }

    // The level the backlight was just given by someone else (the welcome picture switches it): nothing is sent
    void reset(uint8_t level) {
      *this = BacklightFade();
      level_ = level;
    }

    void set(uint8_t level);   // the backlight at this level now (setBacklight() only when it changes)

    // The backlight comes up from its level, in BACKLIGHT_FADE_IN_MS
    void fadeIn() {
      phase_ = IN;
      started_ = false;
    }

    // The screen goes dark, in BACKLIGHT_FADE_OUT_MS from full (nothing new if it goes dark already)
    void fadeOut();

    // Called on every pass: the next level. true once, when the fade out is over (the backlight is off): the caller draws the new
    // screen, then calls fadeIn().
    bool step(uint32_t now);

  private:
    uint8_t phase_ = STEADY;
    bool started_ = false;   // the start time is taken by the first step() of the phase
    uint8_t level_ = 0;      // the level of the backlight now
    uint8_t from_ = 0;       // the level the fade out started from
    uint32_t start_ = 0;
};
