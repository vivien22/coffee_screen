// FlowRecorder.cpp: see FlowRecorder.h

#include "FlowRecorder.h"

void FlowRecorder::begin(uint32_t now) {
  count_ = 0;
  recording_ = true;
  nextAt_ = now + CURVE_SAMPLE_MS;
  lastIn_ = 0;
  lastOut_ = 0;
}

// The water since the previous sample, in tenths of ml. A volume that goes back (noise) gives 0 and is not counted twice; more than 255
// (25.5 ml in one sample: not possible with this pump) is cut, the rest comes with the next sample.
uint8_t FlowRecorder::takeDelta(float ml, int32_t &last) {
  const int32_t now = ml <= 0.0f ? 0 : (int32_t)(ml * 10.0f + 0.5f);
  int32_t delta = now - last;
  if (delta <= 0) {
    return 0;
  }
  if (delta > 255) {
    delta = 255;
  }
  last += delta;
  return (uint8_t)delta;
}

void FlowRecorder::update(uint32_t now, float inMl, float outMl) {
  // One sample per interval; if the loop was late, the samples that were due come at once (the water is in the first one)
  while (recording_ && (int32_t)(now - nextAt_) >= 0) {
    if (count_ >= CURVE_SAMPLES) {
      recording_ = false;   // full
      return;
    }
    samples_[2 * count_] = takeDelta(inMl, lastIn_);
    samples_[2 * count_ + 1] = takeDelta(outMl, lastOut_);
    count_++;
    nextAt_ += CURVE_SAMPLE_MS;
  }
}
