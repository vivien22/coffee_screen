/*
  BoilerRefill: the boiler is refilled when the steaming mode is left (the steam took some of its water). The pump runs at full
  power with the valve closed, so the water can only go into the boiler. When the boiler is full its pressure rises to the OPV
  valve, which sends the water back to the tank through the Out sensor: the refill stops when the Out flow reaches
  BOILER_REFILL_FULL_RATIO of the In flow. It also stops after BOILER_REFILL_MAX_ML (the steam wand left open), BOILER_REFILL_MAX_MS,
  or without water (less than BOILER_REFILL_NO_FLOW_ML after BOILER_REFILL_NO_FLOW_MS).

  Pure logic (no Arduino): the time and the volumes In and Out are given by the caller (test/test_boiler_refill.cpp).
*/

#pragma once

#include <stdint.h>

enum RefillEnd : uint8_t { REFILL_RUNNING, REFILL_FULL, REFILL_MAX_VOLUME, REFILL_MAX_TIME, REFILL_NO_WATER, REFILL_STOPPED };

class BoilerRefill {
  public:
    void begin(uint32_t now, float inMl, float outMl);

    // Called on every pass: true while the pump must run
    bool update(uint32_t now, float inMl, float outMl);

    void stop();   // stopped from outside (a push of the user, the steaming mode entered again)

    bool active() const { return end_ == REFILL_RUNNING && started_; }
    RefillEnd end() const { return end_; }
    float netMl() const { return netMl_; }   // In - Out since begin(): the water that stayed in the boiler

  private:
    void finish(RefillEnd end) { end_ = end; }

    bool started_ = false;
    RefillEnd end_ = REFILL_STOPPED;
    uint32_t startedAt_ = 0;
    uint32_t windowAt_ = 0;
    float in0_ = 0.0f;
    float out0_ = 0.0f;
    float inWindow_ = 0.0f;
    float outWindow_ = 0.0f;
    float netMl_ = 0.0f;
};
