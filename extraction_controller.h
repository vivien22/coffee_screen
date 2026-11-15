/*
  extraction_controller: the extraction state machine and its actuators:
  the pump relay and the 3-way solenoid valve relay.
  It uses the flow sensor to know the volume and the flow.
  Knows nothing about the encoder, the heater or the screen.

  Phases of an extraction (the pre-infusion phases are optional, see config.h):

      IDLE ──start()──> PREINFUSION ──> PREINFUSION_WAIT ──> RUNNING ──> FINISHED
                        (wetting)       (soaking)             (main      or ERROR
                                                               extraction)
      stop() can be called in any phase.

      State              Pump   3-way valve               Ends when
      -----------------  -----  ------------------------  --------------------------------
      IDLE / FINISHED /  OFF    closed (group head is     -
      ERROR                     vented to the drip tray)
      PREINFUSION        ON     open (water to the puck)  PREINFUSION_TIME_MS elapsed
      PREINFUSION_WAIT   OFF    open (pressure is held)   PREINFUSION_WAITING_TIME_MS elapsed
      RUNNING            ON     open                      target volume reached

  Whatever the phase, the extraction also ends if the target volume is reached
  (the volume counts the water of the pre-infusion too), after DURATION_MAX seconds,
  or when stop() is called.
*/

#pragma once
#include <Arduino.h>
#include "USN_FlowSensor.h"

enum ExtractionState : uint8_t {
  EXTRACTION_IDLE,
  EXTRACTION_PREINFUSION,        // pump on, valve open: the puck is wetted
  EXTRACTION_PREINFUSION_WAIT,   // pump off, valve open: the puck soaks
  EXTRACTION_RUNNING,            // pump on, valve open: main extraction
  EXTRACTION_FINISHED,
  EXTRACTION_ERROR
};

class ExtractionController {
  public:
    ExtractionController(uint8_t pumpPin, uint8_t valvePin, USN_FlowSensor &flow);

    // The pump and the valve are switched off.
    void begin();

    // Start a new extraction: first phase is PREINFUSION (or RUNNING if the pre-infusion
    // is disabled in config.h). Ignored if an extraction is already in progress.
    // pressTime: millis() value of the push that started the extraction (timer start).
    void start(uint32_t pressTime);

    // End the extraction in any phase: the pump is switched off, then the valve closes.
    // finalState: EXTRACTION_FINISHED (normal end) or EXTRACTION_ERROR.
    void stop(ExtractionState finalState);

    // Call on every loop pass (also updates the flow sensor): sequences the phases,
    // ends the extraction when targetVolumeMl is reached.
    void update(uint32_t now, uint16_t targetVolumeMl);

    ExtractionState state() const { return state_; }

    // True from start() to stop(), whatever the phase (pre-infusion included),
    // even while the pump is stopped during PREINFUSION_WAIT.
    bool isActive() const {
      return state_ == EXTRACTION_PREINFUSION ||
             state_ == EXTRACTION_PREINFUSION_WAIT ||
             state_ == EXTRACTION_RUNNING;
    }

    // Values of the current (or last) extraction. The duration is counted from the push
    // that started the extraction, so it includes the pre-infusion.
    uint16_t volumeMl() const { return volumeMl_; }
    uint16_t durationSeconds() const { return durationSeconds_; }

    // Seconds left in the pre-infusion phase in progress, rounded up (5, 4, 3, 2, 1), updated by
    // update(). 0 outside PREINFUSION and PREINFUSION_WAIT: the screen shows it instead of the
    // duration while the pre-infusion is in progress.
    uint16_t phaseSecondsLeft() const { return phaseSecondsLeft_; }

    // Flow of the water leaving the boiler, 0 when no extraction is in progress
    float outflowMlPerSec();

  private:
    // Enter a phase: set the pump and the valve, restart the phase timer.
    void enterPhase(ExtractionState phase, uint32_t now);

    void setPump(bool on);
    void setValve(bool open);

    uint8_t pumpPin_;
    uint8_t valvePin_;
    USN_FlowSensor &flow_;

    ExtractionState state_;
    uint32_t startedAt_;        // start of the extraction (push time): total duration
    uint32_t phaseStartedAt_;   // start of the current phase: PREINFUSION / PREINFUSION_WAIT timers
    uint32_t lastVolumeAt_;
    uint16_t volumeMl_;
    uint16_t durationSeconds_;
    uint16_t phaseSecondsLeft_;
};
