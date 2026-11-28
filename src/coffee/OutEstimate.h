/*
  OutEstimate: the water that the OPV valve sends back to the tank, estimated from the flow In alone, while there is no Out sensor
  (FLOW_OUT_SENSOR not defined in Config.h).

  The vibration pump gives less water as the pressure rises; once the pressure reaches the OPV, the OPV sends a part back to the tank.
  The drop of the flow In below the free flow of the pump tells how much:

      Out (ml/s) = OUT_ESTIMATE_GAIN x (OUT_ESTIMATE_FREE_FLOW - flow In)      (0 when the flow In is at the free flow or above)

  Fitted on extraction #2 of the journal (2026-11-26, before the Out sensor melted): over the part where the estimate applies, 35.0 ml
  measured and 35.2 ml estimated (Out 41.9 ml in all: the starts of the phases are not estimated). One extraction only: to check with
  weighed cups (CAL). The flow In is taken over the last OUT_ESTIMATE_WINDOW_MS. The caller says when the estimate applies: only while
  the pump runs at full power in a pressure phase (RUNNING or MANUAL, no flow limit), after OUT_ESTIMATE_SETTLE_MS (the pump that
  starts gives a low flow that is not the pressure).

  Pure logic (no Arduino): the time and the volume In are given by the caller (test/test_out_estimate.cpp).
*/

#pragma once
#include <stdint.h>

class OutEstimate {
  public:
    // A new extraction: nothing estimated yet
    void reset();

    // Called on every pass. inMl: the volume In of the flow sensor. active: the estimate applies now (see above).
    void update(uint32_t now, float inMl, bool active);

    float ml() const { return ml_; }                    // water sent back since reset()
    float rateMlPerS() const { return rate_; }          // its flow now (0 when not active)

  private:
    static const uint8_t SAMPLES = 5;   // the volume In every OUT_ESTIMATE_WINDOW_MS / 4: the window spans 4 intervals
    float samples_[SAMPLES] = {};
    uint8_t count_ = 0;                 // samples taken (up to SAMPLES)
    uint8_t next_ = 0;                  // where the next one goes
    uint32_t sampledAt_ = 0;
    uint32_t lastAt_ = 0;
    bool started_ = false;
    float ml_ = 0.0f;
    float rate_ = 0.0f;
};
