/*
  FlowSimulator: replaces USN_FlowSensor when the flow sensors are not connected (see FLOW_SIMULATOR
  in Config.h). Same interface; no input is read.

  The simulated water follows the pump relay: while the pump is on, water enters the boiler at
  FLOW_SIM_PUMP_ML_PER_S and a part of it (FLOW_SIM_OPV_RATIO) comes back through the OPV valve.
  The pump state is read from its output pin (digitalRead of an OUTPUT pin gives the state of the
  relay), so the simulator needs no link with the extraction controller.
*/

#pragma once
#include <Arduino.h>
#include "../Config.h"

class FlowSimulator {
  public:
    // Same as USN_FlowSensor
    void begin() { lastUpdateAt_ = millis(); }

    // Call on every loop pass: integrates the simulated flow
    void update() {
      const uint32_t now = millis();
      const uint32_t durationMs = now - lastUpdateAt_;
      if (durationMs < FLOW_UPDATE_MS) {
        return;
      }
      lastUpdateAt_ = now;

      const bool pumpOn = (digitalRead(PIN_PUMP_RELAY) == HIGH);
      const float inMlPerSec = pumpOn ? FLOW_SIM_PUMP_ML_PER_S : 0.0f;
      const float outMlPerSec = inMlPerSec * FLOW_SIM_OPV_RATIO;
      const float seconds = durationMs / 1000.0f;

      lastInRate_ = inMlPerSec * 60.0f;     // mL/min, like USN_FlowSensor
      lastOutRate_ = outMlPerSec * 60.0f;
      volumeIn_ += inMlPerSec * seconds;
      volumeOut_ += outMlPerSec * seconds;
    }

    void startExtraction() { resetVolume(); }
    void stopExtraction() { lastInRate_ = 0.0f; lastOutRate_ = 0.0f; }
    void resetVolume() {
      volumeIn_ = 0.0f; volumeOut_ = 0.0f; lastInRate_ = 0.0f; lastOutRate_ = 0.0f;
      lastUpdateAt_ = millis();
    }

    // Flow rates in mL/min, volumes in mL
    float getInflowRate() { return lastInRate_; }
    float getOutflowRate() { return lastOutRate_; }
    float getVolumeIn() { return volumeIn_; }
    float getVolumeOut() { return volumeOut_; }
    float getNetVolume() { return volumeIn_ - volumeOut_; }
    float getNetVolumeLive() { return getNetVolume(); }   // no pulses here: the same

  private:
    uint32_t lastUpdateAt_ = 0;
    float volumeIn_ = 0.0f;
    float volumeOut_ = 0.0f;
    float lastInRate_ = 0.0f;
    float lastOutRate_ = 0.0f;
};
