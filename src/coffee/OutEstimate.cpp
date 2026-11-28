// OutEstimate.cpp: see OutEstimate.h

#include "OutEstimate.h"
#include "../Config.h"

void OutEstimate::reset() {
  *this = OutEstimate();
}

void OutEstimate::update(uint32_t now, float inMl, bool active) {
  const uint32_t step = OUT_ESTIMATE_WINDOW_MS / (SAMPLES - 1);

  // The volume In, sampled every step: the flow In is taken over the window (the oldest sample to the newest)
  if (!started_ || (uint32_t)(now - sampledAt_) >= step) {
    samples_[next_] = inMl;
    next_ = (next_ + 1) % SAMPLES;
    if (count_ < SAMPLES) {
      count_++;
    }
    sampledAt_ = now;
  }
  const uint32_t elapsed = started_ ? now - lastAt_ : 0;
  started_ = true;
  lastAt_ = now;

  rate_ = 0.0f;
  if (!active || count_ < SAMPLES) {
    return;
  }
  const float oldest = samples_[next_];                         // the next slot to be written is the oldest sample
  const float newest = samples_[(next_ + SAMPLES - 1) % SAMPLES];
  const float flowIn = (newest - oldest) * 1000.0f / OUT_ESTIMATE_WINDOW_MS;
  if (flowIn > OUT_ESTIMATE_MIN_FLOW && flowIn < OUT_ESTIMATE_FREE_FLOW) {
    rate_ = OUT_ESTIMATE_GAIN * (OUT_ESTIMATE_FREE_FLOW - flowIn);
    ml_ += rate_ * elapsed / 1000.0f;
  }
}
