// FlowLimit.cpp: see FlowLimit.h
#ifdef FLOW_LIMIT

#include "FlowLimit.h"
#include "../Config.h"

static const float MIN_ML_PER_CLICK = 0.05f;   // the measured flow per click is kept in this range
static const float MAX_ML_PER_CLICK = 0.60f;
static const float CLICKS_PER_SECOND_MAX = 1000.0f / FLOW_LIMIT_SLOT_MS;   // one click per slot: 50 per second at 50 Hz

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

void FlowLimit::begin(float limitMlPerS, uint32_t now, float inMl) {
  limit_ = limitMlPerS;
  mlPerClick_ = FLOW_LIMIT_START_ML_PER_CLICK;
  duty_ = clampf(limit_ / (mlPerClick_ * CLICKS_PER_SECOND_MAX), 0.0f, 1.0f);
  acc_ = 0.0f;
  slotStart_ = now;
  windowStart_ = now;
  inAtWindow_ = inMl;
  clicksWindow_ = 0;
  on_ = false;
}

bool FlowLimit::tick(uint32_t now, float inMl) {
  // The window: the ratio flow per click is updated from the water and the clicks of the window. A window lasts at least
  // FLOW_LIMIT_WINDOW_MS, and goes on until it has FLOW_LIMIT_MIN_CLICKS clicks: at a low limit the pump clicks 2 or 3 times a
  // second, and a one second window would never give a measure.
  if ((uint32_t)(now - windowStart_) >= FLOW_LIMIT_WINDOW_MS && clicksWindow_ >= FLOW_LIMIT_MIN_CLICKS) {
    const float entered = inMl - inAtWindow_;
    if (entered >= 0.0f) {
      const float measured = entered / (float)clicksWindow_;
      mlPerClick_ = clampf(mlPerClick_ + FLOW_LIMIT_GAIN * (measured - mlPerClick_), MIN_ML_PER_CLICK, MAX_ML_PER_CLICK);
    }
    duty_ = clampf(limit_ / (mlPerClick_ * CLICKS_PER_SECOND_MAX), 0.0f, 1.0f);
    windowStart_ = now;
    inAtWindow_ = inMl;
    clicksWindow_ = 0;
  }

  // The slots: one every FLOW_LIMIT_SLOT_MS. After a long stall of the loop, the slots are not replayed (no burst).
  if ((uint32_t)(now - slotStart_) >= 10UL * FLOW_LIMIT_SLOT_MS) {
    slotStart_ = now;
  }
  while ((uint32_t)(now - slotStart_) >= FLOW_LIMIT_SLOT_MS) {
    slotStart_ += FLOW_LIMIT_SLOT_MS;
    acc_ += duty_;
    on_ = (acc_ >= 1.0f);
    if (on_) {
      acc_ -= 1.0f;
      clicksWindow_++;
    }
  }
  return on_;
}

#endif  // FLOW_LIMIT
