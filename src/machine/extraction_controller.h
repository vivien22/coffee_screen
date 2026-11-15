/*
  extraction_controller: the extraction state machine and its actuators:
  the pump relay and the 3-way solenoid valve relay.
  It uses the flow sensor to know the volume and the flow.
  Knows nothing about the encoder, the heater or the screen.

  Phases of an extraction (the pre-infusion and flush phases are optional, see config.h):

      IDLE ──start()──> PREINFUSION ──> PREINFUSION_WAIT ──> RUNNING ──finish()──> FLUSH_WAIT
                        (wetting)       (soaking)             (main                (waits for
                                                               extraction)          startFlush())
                                                                                       │
                                                         FINISHED <── FLUSHING <───────┘
                                                         (or ERROR)   (rinsing)

      finish() is the normal end of the extraction (target volume reached, or stopped by the
      user): it goes to FLUSH_WAIT, or straight to FINISHED if the flush time is 0.
      stop() ends everything at once, in any phase, without flush.

      State              Pump   3-way valve               Ends when
      -----------------  -----  ------------------------  --------------------------------
      IDLE / FINISHED /  OFF    closed (group head is     -
      ERROR                     vented to the drip tray)
      PREINFUSION        ON     open (water to the puck)  pre-infusion time elapsed
      PREINFUSION_WAIT   OFF    open (pressure is held)   wait time elapsed
      RUNNING            ON     open                      target volume reached
      FLUSH_WAIT         OFF    closed                    startFlush() (short push of the user)
      FLUSHING           ON     open                      flush time elapsed

  The three times are settings given to start(): a pre-infusion time of 0 skips the
  whole pre-infusion (the wait too), a wait time of 0 skips only PREINFUSION_WAIT, a flush time
  of 0 skips the whole flush (FLUSH_WAIT too).

  Until RUNNING ends, the extraction also ends if the target volume is reached
  (the volume counts the water of the pre-infusion too), after DURATION_MAX seconds,
  or when stop() is called. The volume and the duration of the extraction are final at the end
  of RUNNING: the flush does not change them.
*/

#pragma once
#include <Arduino.h>
#include "../lib/USN_FlowSensor.h"

enum ExtractionState : uint8_t {
  EXTRACTION_IDLE,
  EXTRACTION_PREINFUSION,        // pump on, valve open: the puck is wetted
  EXTRACTION_PREINFUSION_WAIT,   // pump off, valve open: the puck soaks
  EXTRACTION_RUNNING,            // pump on, valve open: main extraction
  EXTRACTION_FLUSH_WAIT,         // pump off, valve closed: waits for a short push to start the flush
  EXTRACTION_FLUSHING,           // pump on, valve open: the group head is rinsed
  EXTRACTION_FINISHED,
  EXTRACTION_ERROR
};

class ExtractionController {
  public:
    ExtractionController(uint8_t pumpPin, uint8_t valvePin, USN_FlowSensor &flow);

    // The pump and the valve are switched off.
    void begin();

    // Start a new extraction: first phase is PREINFUSION (or RUNNING if the pre-infusion time is 0).
    // Ignored if an extraction is already in progress.
    // pressTime: millis() value of the push that started the extraction (timer start).
    // preinfusionSeconds, waitSeconds: durations of PREINFUSION and PREINFUSION_WAIT (0 = skipped).
    // flushSeconds: duration of FLUSHING (0 = no flush).
    void start(uint32_t pressTime, uint16_t preinfusionSeconds, uint16_t waitSeconds, uint16_t flushSeconds);

    // Normal end of the extraction, from PREINFUSION, PREINFUSION_WAIT or RUNNING: the pump is
    // switched off, then the valve closes, the final values are kept. Next phase is FLUSH_WAIT,
    // or FINISHED if the flush time is 0. Ignored in any other state.
    void finish(uint32_t now);

    // FLUSH_WAIT -> FLUSHING (pump on, valve open for the flush time). Ignored in any other state.
    void startFlush(uint32_t now);

    // End the extraction in any phase, flush included, without flush: the pump is switched off,
    // then the valve closes.
    // finalState: EXTRACTION_FINISHED (stopped by the user) or EXTRACTION_ERROR.
    void stop(ExtractionState finalState);

    // Call on every loop pass (also updates the flow sensor): sequences the phases,
    // ends the extraction when targetVolumeMl is reached.
    void update(uint32_t now, uint16_t targetVolumeMl);

    ExtractionState state() const { return state_; }

    // True from start() to the end of the sequence, whatever the phase (pre-infusion and flush
    // included), even while the pump is stopped (PREINFUSION_WAIT, FLUSH_WAIT).
    bool isActive() const {
      return state_ == EXTRACTION_PREINFUSION ||
             state_ == EXTRACTION_PREINFUSION_WAIT ||
             state_ == EXTRACTION_RUNNING ||
             isFlush();
    }

    // True during FLUSH_WAIT and FLUSHING
    bool isFlush() const {
      return state_ == EXTRACTION_FLUSH_WAIT || state_ == EXTRACTION_FLUSHING;
    }

    // Values of the current (or last) extraction, final from the end of RUNNING. The duration is
    // counted from the push that started the extraction, so it includes the pre-infusion.
    uint16_t volumeMl() const { return volumeMl_; }
    uint16_t durationSeconds() const { return durationSeconds_; }

    // Seconds left in the timed phase in progress, rounded up (5, 4, 3, 2, 1), updated by
    // update(): PREINFUSION, PREINFUSION_WAIT and FLUSHING. In FLUSH_WAIT it is the flush time
    // (what the flush will last): the screen does not show it, it keeps the duration of the
    // extraction. 0 in the other states. The screen shows it instead of the duration during the
    // timed phases.
    uint16_t phaseSecondsLeft() const { return phaseSecondsLeft_; }

    // Flow of the water leaving the boiler, 0 when no extraction is in progress
    float outflowMlPerSec();

  private:
    // Seconds left of a phase (rounded up), given its duration
    uint16_t secondsLeft(uint32_t phaseDurationMs, uint32_t now) const;

    // Enter a phase: set the pump and the valve, restart the phase timer.
    void enterPhase(ExtractionState phase, uint32_t now);

    void setPump(bool on);
    void setValve(bool open);

    uint8_t pumpPin_;
    uint8_t valvePin_;
    USN_FlowSensor &flow_;

    ExtractionState state_;
    uint32_t startedAt_;        // start of the extraction (push time): total duration
    uint32_t phaseStartedAt_;   // start of the current phase: PREINFUSION / PREINFUSION_WAIT / FLUSHING timers
    uint32_t lastVolumeAt_;
    uint16_t volumeMl_;
    uint16_t durationSeconds_;
    uint16_t phaseSecondsLeft_;
    uint16_t preinfusionMs_;    // duration of PREINFUSION, from the settings at start()
    uint16_t waitMs_;           // duration of PREINFUSION_WAIT, from the settings at start()
    uint16_t flushMs_;          // duration of FLUSHING, from the settings at start()
};
