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
  inPeak_ = 0.0f;
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
  } else if ((uint32_t)(now - windowAt_) >= BOILER_REFILL_WINDOW_MS) {
    // The flows of the window. The boiler is full when the OPV sends a good part of the water back (Out), only for the windows that
    // start after BOILER_REFILL_MIN_MS: the Out sensor may still count what the steaming made flow. Or when the flow In drops from the
    // highest one (the pressure rises), from BOILER_REFILL_MIN_MS on.
    const float inWindow = inMl - inWindow_;
    const float outWindow = outMl - outWindow_;
    if (inWindow > inPeak_) {
      inPeak_ = inWindow;
    }
    const bool settled = (uint32_t)(windowAt_ - startedAt_) >= BOILER_REFILL_MIN_MS;
    const bool outFull = settled && inWindow > 0.0f && outWindow >= BOILER_REFILL_FULL_RATIO * inWindow;
    const bool inDrop = elapsed >= BOILER_REFILL_MIN_MS && inPeak_ >= BOILER_REFILL_IN_MIN_ML && inWindow <= BOILER_REFILL_IN_DROP * inPeak_;
    if (outFull || inDrop) {
      finish(REFILL_FULL);
    }
    windowAt_ = now;
    inWindow_ = inMl;
    outWindow_ = outMl;
  }
  return active();
}
