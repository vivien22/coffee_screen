/*
  ExtractionController: the extraction state machine and its actuators:
  the pump relay and the 3-way solenoid valve relay.
  It uses the flow sensors to know the volume in the cup (estimated, see Config.h) and the flow
  of the water entering the boiler.
  Knows nothing about the encoder, the heater or the screen.

  Phases of an extraction (the pre-infusion and flush phases are optional, see Config.h):

      IDLE ──start()──> PREINFUSION ──> PREINFUSION_WAIT ──> RUNNING ──finish()──> FLUSH_WAIT
                        (wetting,       (soaking,             (main                (waits for
                         by volume)      by time)              extraction)          startFlush())
                                                                                       │
                                                         FINISHED <── FLUSHING <───────┘
                                                         (or ERROR)   (rinsing)

      finish() is the normal end of the extraction (target volume reached, or stopped by the
      user): it goes to FLUSH_WAIT, or straight to FINISHED if the flush time is 0.
      stop() ends everything at once, in any phase, without flush.
      Safety: no flow while the pump runs (PREINFUSION, RUNNING) for NO_FLOW_TIMEOUT_MS:
      stop(ERROR_NO_WATER). No extraction can start until clearError() (short push of the encoder).

      State              Pump   3-way valve               Ends when
      -----------------  -----  ------------------------  --------------------------------
      IDLE / FINISHED /  OFF    closed (grouphead is      -
      ERROR                     vented to the drip tray)
      PREINFUSION        ON     open (water to the puck)  pre-infusion volume reached
      PREINFUSION_WAIT   OFF    open (pressure is held)   wait time elapsed
      RUNNING            ON     open                      target volume (in the cup) reached
      FLUSH_WAIT         OFF    closed                    startFlush() (short push of the user)
      FLUSHING           ON     open                      flush time elapsed
      MANUAL             ON     open                      stop() (second push), or the MANUAL time

  The settings given to start(): a pre-infusion volume of 0 skips the whole pre-infusion (the wait
  too), a wait time of 0 skips only PREINFUSION_WAIT, a flush time of 0 skips the whole flush
  (FLUSH_WAIT too).

  PREINFUSION ends when the pre-infusion volume has entered the boiler (In - Out of the flow
  sensors, see Config.h), or after PREINFUSION_TIMEOUT_MS. The target volume (in the cup) is
  checked in RUNNING only; the water of the pre-infusion is counted in the volume, as it stays in
  the puck. RUNNING also ends after DURATION_MAX seconds, or when stop() is called. The volume and
  the duration of the extraction are final at the end of RUNNING: the flush does not change them.
*/

#pragma once
#include <Arduino.h>
#include "../Config.h"
#include "../drivers/FlowSensor.h"
#include "../settings/Journal.h"   // EndReason

// The phases PREINFUSION..MANUAL are described by the phase table of ExtractionController.cpp
// (one row per phase, in this order): keep them together and in the same order as the table.
enum ExtractionState : uint8_t {
  EXTRACTION_IDLE,
  EXTRACTION_PREINFUSION,        // pump on, valve open: the puck is wetted
  EXTRACTION_PREINFUSION_WAIT,   // pump off, valve open: the puck soaks
  EXTRACTION_RUNNING,            // pump on, valve open: main extraction
  EXTRACTION_FLUSH_WAIT,         // pump off, valve closed: waits for a short push to start the flush
  EXTRACTION_FLUSHING,           // pump on, valve open: the grouphead is rinsed
  EXTRACTION_MANUAL,             // pump on, valve open, until stop(): MANUAL mode
  EXTRACTION_FINISHED,
  EXTRACTION_ERROR,              // extraction too long
  EXTRACTION_ERROR_NO_WATER      // no water entering the boiler: kept until clearError()
};

class ExtractionController {
  public:
    ExtractionController(uint8_t pumpPin, uint8_t valvePin, FlowSensor &flow);

    // The pump and the valve are switched off.
    void begin();

    // Start a new extraction: first phase is PREINFUSION (or RUNNING if the pre-infusion time is 0).
    // Ignored if an extraction is already in progress.
    // pressTime: millis() value of the push that started the extraction (timer start).
    // preinfusionMl: water to send during PREINFUSION (0 = no pre-infusion, the wait is skipped too).
    // waitSeconds: duration of PREINFUSION_WAIT (0 = skipped).
    // flushSeconds: duration of FLUSHING (0 = no flush).
    void start(uint32_t pressTime, uint16_t preinfusionMl, uint16_t waitSeconds, uint16_t flushSeconds);

    // MANUAL mode: pump on and valve open until stop(EXTRACTION_FINISHED), or until seconds have
    // elapsed if seconds > 0 (then FINISHED too). No target volume, no pre-infusion, no flush.
    // The volume is In - Out (no loss).
    // Ignored if an extraction is in progress or after a no-water error.
    void startManual(uint32_t pressTime, uint16_t seconds);

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

    // Why the extraction (or the last one) ended, and in which phase: given by whoever ends it, the first reason is kept
    // (the end of a flush does not replace it). END_NONE until it ends. See Journal.h.
    void endedBy(EndReason reason);
    EndReason endReason() const { return (EndReason)endReason_; }
    uint8_t endPhase() const { return endPhase_; }
    bool isManual() const { return manual_; }
    float loss() const { return lossMl_; }
    // The water counted by the sensors, final like volumeMl() (not those of a flush)
    float volumeInMl() const { return finalInMl_; }
    float volumeOutMl() const { return finalOutMl_; }

    // No-water error: the pump is stopped, the state stays EXTRACTION_ERROR_NO_WATER and start() is
    // ignored until clearError() puts the controller back to IDLE (ignored in any other state).
    bool hasNoWaterError() const { return state_ == EXTRACTION_ERROR_NO_WATER; }
    void clearError();

    // True from start() to the end of the sequence, whatever the phase (pre-infusion and flush
    // included), even while the pump is stopped (PREINFUSION_WAIT, FLUSH_WAIT).
    bool isActive() const {
      return state_ >= EXTRACTION_PREINFUSION && state_ <= EXTRACTION_MANUAL;   // the phases of the table
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
    // update(): PREINFUSION_WAIT, FLUSHING and MANUAL with a time (0 without time: nothing to count down). In FLUSH_WAIT it is the flush time (what the flush
    // will last): the screen does not show it, it keeps the duration of the extraction. 0 in the
    // other states. The screen shows it instead of the duration during these phases.
    uint16_t phaseSecondsLeft() const { return phaseSecondsLeft_; }

    // Millilitres left to send in PREINFUSION, rounded up (10, 9, ... 1), updated by update().
    // 0 in the other states. The screen shows it instead of the volume during the pre-infusion.
    uint16_t preinfusionMlLeft() const { return preinfusionMlLeft_; }

    // Flow of the water entering the boiler (the water to heat), 0 when no extraction is in progress
    float inflowMlPerSec();

    // Water that does not end in the cup (puck, backflush), see Config.h: the setting of the user,
    // given by the machine. Used to estimate the volume in the cup.
    void setLossMl(float ml) { lossMl_ = ml; }

    // The delay (seconds of flow) by which the pump is stopped before the target, see STOP_DELAY_* in Config.h. The machine
     // gives the saved one at the start-up and saves the new one when stopDelayLearned() says it changed.
    void setStopDelaySeconds(float seconds) { stopDelayS_ = seconds; }
    float stopDelaySeconds() const { return stopDelayS_; }
    bool stopDelayLearned() { const bool was = stopDelayChanged_; stopDelayChanged_ = false; return was; }

    // True while the water that comes after a stop is being measured (STOP_SETTLE_MS): the final volume is not known yet
    bool settlePending() const { return settlePending_; }
    // True once when that measure is over and volumeMl() / netVolumeMl() have been brought to the final values (the screen
    // shows them again)
    bool takeSettled() { const bool was = settled_; settled_ = false; return was; }

    // In - Out at the end of the extraction (final like volumeMl()), and whether this extraction can be
    // used to correct the loss: an AUTO extraction ended normally (finish()) that gave at least
    // CALIBRATION_MIN_ML. False after an abort, an error, a MANUAL extraction, until the next start().
    float netVolumeMl() const { return netMl_; }
    bool calibratable() const { return finishedNormally_ && !manual_ && volumeMl_ >= CALIBRATION_MIN_ML; }

  private:
    // Seconds left of a phase (rounded up), given its duration
    uint16_t secondsLeft(uint32_t phaseDurationMs, uint32_t now) const;

    // Duration (ms) of the phase in progress, from the phase table; 0 = no time limit
    uint32_t phaseLimitMs() const;

    // Common part of start() and startManual(): false if no extraction can start
    bool beginExtraction_(uint32_t pressTime);

    // Enter a phase: set the pump and the valve, restart the phase timer.
    void enterPhase(ExtractionState phase, uint32_t now);

    // Volume in the cup estimated from the flow sensors: (In - Out) - loss, >= 0 (also keeps In - Out in netMl_)
    uint16_t cupVolumeMl();

    // The same at this very moment (the pulses not yet converted included), not rounded, can be negative
    float cupVolumeLiveMl();

    // Flow of In - Out (ml/s) and the volume still to come after the pump stops (see STOP_DELAY_* in Config.h)
    float netRateMlPerSec();
    float anticipationMl();

    // After finish(): waits STOP_SETTLE_MS, learns the delay from the water that came after the stop, and brings the final
    // volume of the extraction up to date
    void settle(uint32_t now);

    void setPump(bool on);
    void setValve(bool open);

    uint8_t pumpPin_;
    uint8_t valvePin_;
    FlowSensor &flow_;

    ExtractionState state_;
    uint32_t startedAt_;        // start of the extraction (push time): total duration
    uint32_t phaseStartedAt_;   // start of the current phase: PREINFUSION / PREINFUSION_WAIT / FLUSHING timers
    uint32_t lastVolumeAt_;
    uint16_t volumeMl_;         // estimated volume in the cup
    uint16_t durationSeconds_;
    uint16_t phaseSecondsLeft_;
    uint16_t preinfusionMl_;    // volume of PREINFUSION, from the settings at start()
    uint16_t preinfusionMlLeft_;
    uint8_t phaseSeconds_[5];   // durations of the timed phases (s), indexed by PhaseDuration (see the .cpp)
    bool manual_;               // the extraction in progress (or the last one) is a MANUAL one
    float lossMl_;              // water that does not end in the cup (setting)
    float netMl_;               // In - Out when volumeMl_ was last computed
    bool finishedNormally_;     // the last extraction ended with finish()
    uint8_t endReason_;         // why it ended (EndReason) and in which phase, see endedBy()
    uint8_t endPhase_;
    float finalInMl_;           // In and Out when the volume was last made final (stop() and settle())
    float finalOutMl_;
    float flowCheckMl_;         // volume In at the last progress seen (no-flow safety)
    uint32_t flowCheckAt_;      // time of that progress

    // The stop before the target
    float stopDelayS_;          // seconds of flow the pump is stopped before the target (learned)
    bool settlePending_;        // after finish(): the water that comes after the stop is measured
    bool learning_;             // ... and the delay learns from it
    bool settled_;              // the measure is over (given once by takeSettled())
    bool stopDelayChanged_;     // the delay was learned (given once by stopDelayLearned())
    float stopNetMl_;           // In - Out when the pump was stopped
    float stopNetRate_;         // its flow (ml/s) at that moment
    uint32_t settleAt_;         // time of the stop
};
