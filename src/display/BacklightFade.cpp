// BacklightFade.cpp: see BacklightFade.h

#include "BacklightFade.h"
#include "../Config.h"
#include "Display.h"   // setBacklight()

void BacklightFade::set(uint8_t level) {
  if (level != level_) {
    level_ = level;
    setBacklight(level);
  }
}

void BacklightFade::fadeOut() {
  if (phase_ != OUT) {   // (during a fade out the new screen is already waiting)
    phase_ = OUT;
    started_ = false;
    from_ = level_;
  }
}

bool BacklightFade::step(uint32_t now) {
  if (phase_ == STEADY) {
    return false;
  }
  if (!started_) {
    started_ = true;
    start_ = now;
  }
  const uint32_t elapsed = now - start_;

  if (phase_ == OUT) {
    const uint32_t span = (uint32_t)BACKLIGHT_FADE_OUT_MS * from_ / 255;
    if (elapsed < span) {
      set((uint8_t)(from_ * (span - elapsed) / span));
      return false;
    }
    set(0);
    return true;   // dark: the new screen is drawn now
  }

  if (elapsed < BACKLIGHT_FADE_IN_MS) {
    set((uint8_t)(255UL * elapsed / BACKLIGHT_FADE_IN_MS));
  } else {
    set(255);
    phase_ = STEADY;
  }
  return false;
}
