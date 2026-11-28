// DigitRoll.cpp: see DigitRoll.h

#include "DigitRoll.h"
#include "../Config.h"
#include "Display.h"   // DISPLAY_ROLL_DONE

// The progress (0 to DISPLAY_ROLL_DONE - 1) of a roll that has lasted elapsed ms of duration: ease-out, fast first, slow at the end
static uint8_t rollProgress(uint32_t elapsed, uint32_t duration) {
  if (duration == 0) {
    return DISPLAY_ROLL_DONE - 1;   // no roll (ROLL_VOLUME_MS is 0): never reached, it only keeps the division safe
  }
  const uint32_t remaining = DISPLAY_ROLL_DONE - elapsed * DISPLAY_ROLL_DONE / duration;   // 255 at the start, 0 at the end
  const uint32_t cubed = remaining * remaining * remaining / (DISPLAY_ROLL_DONE * DISPLAY_ROLL_DONE);
  return (uint8_t)(DISPLAY_ROLL_DONE - cubed);
}

bool DigitRoll::start(uint32_t now, uint32_t durationMs, int16_t &finishValue) {
  const bool finishFirst = active_ && finishFirst_;
  finishValue = finishValue_;
  requested_ = finishFirst_ = false;
  active_ = durationMs > 0;
  start_ = now;
  nextFrame_ = now + ROLL_FRAME_MS;
  return finishFirst;
}

bool DigitRoll::frame(uint32_t now, uint32_t durationMs, uint8_t &progress) {
  if (!active_ || (int32_t)(now - nextFrame_) < 0) {
    return false;
  }
  const uint32_t elapsed = now - start_;
  if (elapsed >= durationMs) {
    active_ = false;
    progress = DISPLAY_ROLL_DONE;
  } else {
    nextFrame_ = now + ROLL_FRAME_MS;
    progress = rollProgress(elapsed, durationMs);
  }
  return true;
}
