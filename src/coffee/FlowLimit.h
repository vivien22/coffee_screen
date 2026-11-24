/*
  FlowLimit: limits the flow of the pump during the main extraction (RUNNING), without a pressure sensor and without a PI.
  Test version, Nano R4 only (built with -DFLOW_LIMIT=1, see platformio.ini). Nothing here is compiled without FLOW_LIMIT.

  The pump is a vibration pump driven by the mains: the SSR only switches at the zero crossings, so time is cut in slots of
  one mains period (FLOW_LIMIT_SLOT_MS, 20 ms at 50 Hz). A slot is ON (one click of the pump) or OFF. The ON slots are spread
  evenly (error diffusion), so the mean power follows the duty.

  The flow per click is not known: it depends on the pressure, which is not measured. It is measured instead, once per
  window: the water that entered the boiler (In, flow sensor) divided by the clicks of that window. The duty that gives the
  limit is then limit / (flow per click * clicks per second), capped at 100 %. No PI: the limit is a ceiling, nothing else.

  Pure logic (no Arduino): the time and the volume In are given by the caller, so the test runs on the PC (test/test_flow_limit.cpp).
*/

#pragma once
#ifdef FLOW_LIMIT

#include <stdint.h>

class FlowLimit {
  public:
    // Start of the RUNNING phase: limitMlPerS is the limit (ml/s, > 0), inMl the volume In of the flow sensor at that moment.
    void begin(float limitMlPerS, uint32_t now, float inMl);

    // Called on every pass of RUNNING. Returns true if the pump is ON now (one slot, see above).
    bool tick(uint32_t now, float inMl);

    float mlPerClick() const { return mlPerClick_; }   // measured flow per click (ml)
    float duty() const { return duty_; }               // 0..1, the fraction of the slots that are ON

  private:
    float limit_ = 0.0f;          // ml/s
    float mlPerClick_ = 0.0f;
    float duty_ = 0.0f;
    float acc_ = 0.0f;            // error diffusion: the part of a slot not given yet
    uint32_t slotStart_ = 0;
    uint32_t windowStart_ = 0;
    float inAtWindow_ = 0.0f;     // volume In at the start of the window
    uint16_t clicksWindow_ = 0;   // ON slots in the window
    bool on_ = false;
};

#endif  // FLOW_LIMIT
