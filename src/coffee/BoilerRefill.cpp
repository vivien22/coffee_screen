// BoilerRefill.cpp: see BoilerRefill.h

#include "BoilerRefill.h"
#include "../Config.h"

void BoilerRefill::begin(uint32_t now, float inMl, float outMl) {
  started_ = true;
  end_ = REFILL_RUNNING;
  startedAt_ = windowAt_ = now;
  in0_ = inWindow_ = inMl;
  out0_ = outWindow_ = outMl;
  netMl_ = 0.0f;
}

void BoilerRefill::stop() {
  if (active()) {
    finish(REFILL_STOPPED);
  }
}

bool BoilerRefill::update(uint32_t now, float inMl, float outMl) {
  if (!active()) {
    return false;
  }
  const uint32_t elapsed = now - startedAt_;
  const float in = inMl - in0_;
  netMl_ = in - (outMl - out0_);

  if (netMl_ >= BOILER_REFILL_MAX_ML) {
    finish(REFILL_MAX_VOLUME);
  } else if (elapsed >= BOILER_REFILL_MAX_MS) {
    finish(REFILL_MAX_TIME);
  } else if (elapsed >= BOILER_REFILL_NO_FLOW_MS && in < BOILER_REFILL_NO_FLOW_ML) {
    finish(REFILL_NO_WATER);
  } else if (elapsed < BOILER_REFILL_MIN_MS || (uint32_t)(now - windowAt_) >= BOILER_REFILL_WINDOW_MS) {
    // The flows of the window: the boiler is full when the OPV sends a good part of the water back (Out). The windows start after
    // BOILER_REFILL_MIN_MS: the Out sensor may still count what the steaming made flow.
    const float inWindow = inMl - inWindow_;
    const float outWindow = outMl - outWindow_;
    if (elapsed >= BOILER_REFILL_MIN_MS && inWindow > 0.0f && outWindow >= BOILER_REFILL_FULL_RATIO * inWindow) {
      finish(REFILL_FULL);
    }
    windowAt_ = now;
    inWindow_ = inMl;
    outWindow_ = outMl;
  }
  return active();
}
