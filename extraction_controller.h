/*
  extraction_controller: the extraction state machine.
  It owns the pump relay and uses the flow sensor to know the volume and the flow.
  It stops by itself when the target volume is reached or after DURATION_MAX seconds.
  Knows nothing about the encoder, the heater or the screen.
*/

#pragma once
#include <Arduino.h>
#include "USN_FlowSensor.h"

enum ExtractionState : uint8_t {
  EXTRACTION_IDLE,
  EXTRACTION_RUNNING,
  EXTRACTION_FINISHED,
  EXTRACTION_ERROR
};

class ExtractionController {
  public:
    ExtractionController(uint8_t pumpPin, USN_FlowSensor &flow);

    // The pump is switched off.
    void begin();

    // pressTime: millis() value of the push that started the extraction (timer start).
    void start(uint32_t pressTime);
    void stop(ExtractionState finalState);

    // Call on every loop pass (also updates the flow sensor).
    // targetVolumeMl: the extraction ends when this volume is reached.
    void update(uint32_t now, uint16_t targetVolumeMl);

    ExtractionState state() const { return state_; }
    bool isRunning() const { return state_ == EXTRACTION_RUNNING; }

    // Values of the current (or last) extraction
    uint16_t volumeMl() const { return volumeMl_; }
    uint16_t durationSeconds() const { return durationSeconds_; }

    // Flow of the water leaving the boiler, 0 when no extraction is running
    float outflowMlPerSec();

  private:
    void setPump(bool on);

    uint8_t pumpPin_;
    USN_FlowSensor &flow_;

    ExtractionState state_;
    uint32_t startedAt_;
    uint32_t lastVolumeAt_;
    uint16_t volumeMl_;
    uint16_t durationSeconds_;
};
