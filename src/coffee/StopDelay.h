/*
  StopDelay: the stop before the target (see "The stop before the target" in docs/DOCUMENTATION.md).

  When the pump stops the water goes on for a moment (the pulses not counted yet, the water in the pipes, the pump slowing down),
  so the pump is stopped a little BEFORE the target: the water still to come is the flow at that moment times a delay (seconds of
  flow, STOP_ANTICIPATION_MAX_ML at most).

  The delay is learned. After a stop at the end of the main phase the water keeps being measured for STOP_SETTLE_MS; the water that
  really came, divided by the flow at the stop, is the delay that would have been right, and the delay moves STOP_DELAY_GAIN of the
  way toward it (kept between 0 and STOP_DELAY_MAX_S). The machine saves it when takeChanged() says it changed.

  Pure logic (no Arduino): the time, the volumes and the flows are given by the caller (ExtractionController; test/test_anticipation.cpp).
*/

#pragma once

#include <stdint.h>

class StopDelay {
  public:
    StopDelay();

    void setSeconds(float seconds) { seconds_ = seconds; }   // the saved delay, at start-up
    float seconds() const { return seconds_; }

    // The volume still to come if the pump stopped now, at netRateMlPerS (In - Out): the pump stops when the volume plus this
    // reaches the target.
    float anticipationMl(float netRateMlPerS) const;

    // The pump just stopped at the end of the main phase: the water that comes from now on is measured against netMl (In - Out at the
    // stop). learn: the flow at the stop (netRateMlPerS) can teach the delay.
    void beginSettle(uint32_t now, float netMl, float netRateMlPerS, bool learn);

    // The pump runs again (the flush, a new extraction): the water that comes is not the one of the stop any more, nothing is learned.
    void cancel();

    // Called on every pass while pending(): true once, when STOP_SETTLE_MS are over; the delay was learned then if it could be.
    // netMl: In - Out at this very moment.
    bool settle(uint32_t now, float netMl);

    bool pending() const { return pending_; }   // the water after a stop is being measured

    // True once after the delay changed (worth a write of the EEPROM)
    bool takeChanged() {
      const bool was = changed_;
      changed_ = false;
      return was;
    }

  private:
    float seconds_;
    bool pending_ = false;
    bool learning_ = false;
    bool changed_ = false;
    float stopNetMl_ = 0.0f;     // In - Out when the pump stopped
    float stopNetRate_ = 0.0f;   // its flow (ml/s) at that moment
    uint32_t stoppedAt_ = 0;
};
