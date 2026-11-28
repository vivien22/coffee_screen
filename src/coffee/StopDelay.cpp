// StopDelay.cpp: see StopDelay.h

#include "StopDelay.h"
#include "../Config.h"

StopDelay::StopDelay() : seconds_(STOP_DELAY_DEFAULT_S) {
}

float StopDelay::anticipationMl(float netRateMlPerS) const {
  const float ml = seconds_ * netRateMlPerS;
  return constrain(ml, 0.0f, (float)STOP_ANTICIPATION_MAX_ML);
}

void StopDelay::beginSettle(uint32_t now, float netMl, float netRateMlPerS, bool learn) {
  pending_ = true;
  learning_ = learn;
  stoppedAt_ = now;
  stopNetMl_ = netMl;
  stopNetRate_ = netRateMlPerS;
}

void StopDelay::cancel() {
  pending_ = false;
  learning_ = false;
}

bool StopDelay::settle(uint32_t now, float netMl) {
  if (!pending_ || (uint32_t)(now - stoppedAt_) < STOP_SETTLE_MS) {
    return false;
  }
  pending_ = false;

  if (learning_) {
    learning_ = false;
    const float came = netMl - stopNetMl_;   // the water that came after the stop
    if (came >= 0.0f) {
      const float wanted = constrain(came / stopNetRate_, 0.0f, (float)STOP_DELAY_MAX_S);
      const float next = seconds_ + STOP_DELAY_GAIN * (wanted - seconds_);
      if (fabsf(next - seconds_) >= 0.005f) {   // not worth a write of the EEPROM below
        seconds_ = next;
        changed_ = true;
      }
    }
  }
  return true;
}
