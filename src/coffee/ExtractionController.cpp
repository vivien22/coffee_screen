// ExtractionController.cpp: phases of an extraction, safeties (no flow, maximum duration) and the volume estimate.

#include "ExtractionController.h"
#include "../Config.h"
#include "../Debug.h"

// ------------------------------------------------------------
// Phase table: what each phase does and how it ends. The code below only reads it.
//
//   phase              pump  valve  ends on                      then
//   PREINFUSION        on    open   its volume (or 30 s)         PREINFUSION_WAIT (RUNNING if the wait is 0)
//   PREINFUSION_WAIT   off   open   the wait time                RUNNING
//   RUNNING            on    open   the target volume            finish(): FLUSH_WAIT or FINISHED
//   FLUSH_WAIT         off   closed startFlush() (a push)        FLUSHING
//   FLUSHING           on    open   the flush time               FINISHED
//   MANUAL             on    open   the MANUAL time (0 = never)  FINISHED
//
// The volume conditions (PREINFUSION, RUNNING) and the pushes are handled in update() / by the machine.

// Which duration limits a phase: index in phaseSeconds_[]
enum PhaseDuration : uint8_t {
  DURATION_NONE,                 // always 0: no time limit
  DURATION_PREINFUSION_TIMEOUT,  // PREINFUSION_TIMEOUT_MS (safety)
  DURATION_WAIT,                 // setting: wait after the pre-infusion
  DURATION_FLUSH,                // setting: flush
  DURATION_MANUAL                // setting: MANUAL time (0 = until the push)
};

enum PhaseFlags : uint8_t {
  PHASE_PUMP       = 0x01,   // pump on
  PHASE_VALVE      = 0x02,   // valve open
  PHASE_TIMED      = 0x04,   // ends when its duration has elapsed (if the duration is not 0)
  PHASE_COUNTDOWN  = 0x08,   // the seconds left are given to the screen
  PHASE_FLOW_CHECK = 0x10,   // the no-flow safety watches this phase
  PHASE_FLUSH      = 0x20    // after the extraction: its volume and duration are final
};

struct PhaseRow {
  uint8_t flags;
  uint8_t duration;   // PhaseDuration
  uint8_t next;       // state entered when the duration has elapsed
};

// One row per phase, from EXTRACTION_PREINFUSION to EXTRACTION_MANUAL (same order as the enum)
static const PhaseRow PHASES[] PROGMEM = {
  /* PREINFUSION      */ { PHASE_PUMP | PHASE_VALVE | PHASE_TIMED | PHASE_FLOW_CHECK,                   DURATION_PREINFUSION_TIMEOUT, EXTRACTION_PREINFUSION_WAIT },
  /* PREINFUSION_WAIT */ { PHASE_VALVE | PHASE_TIMED | PHASE_COUNTDOWN,                                 DURATION_WAIT,                EXTRACTION_RUNNING },
  /* RUNNING          */ { PHASE_PUMP | PHASE_VALVE | PHASE_FLOW_CHECK,                                 DURATION_NONE,                EXTRACTION_FINISHED },
  /* FLUSH_WAIT       */ { PHASE_COUNTDOWN | PHASE_FLUSH,                                               DURATION_FLUSH,               EXTRACTION_FLUSHING },
  /* FLUSHING         */ { PHASE_PUMP | PHASE_VALVE | PHASE_TIMED | PHASE_COUNTDOWN | PHASE_FLUSH,      DURATION_FLUSH,               EXTRACTION_FINISHED },
  /* MANUAL           */ { PHASE_PUMP | PHASE_VALVE | PHASE_TIMED | PHASE_COUNTDOWN | PHASE_FLOW_CHECK, DURATION_MANUAL,              EXTRACTION_FINISHED },
};

// Fields of the row of an active phase
static inline const PhaseRow *rowOf(ExtractionState phase) { return &PHASES[phase - EXTRACTION_PREINFUSION]; }
static inline uint8_t flagsOf(ExtractionState phase)    { return pgm_read_byte(&rowOf(phase)->flags); }
static inline uint8_t durationOf(ExtractionState phase) { return pgm_read_byte(&rowOf(phase)->duration); }
static inline uint8_t nextOf(ExtractionState phase)     { return pgm_read_byte(&rowOf(phase)->next); }

// ------------------------------------------------------------

// The controller drives the pump and the valve relays; the flow sensor is shared with the rest of the machine.
ExtractionController::ExtractionController(uint8_t pumpPin, uint8_t valvePin, FlowSensor &flow) :
  pumpPin_(pumpPin),
  valvePin_(valvePin),
  flow_(flow),
  state_(EXTRACTION_IDLE),
  startedAt_(0),
  phaseStartedAt_(0),
  lastVolumeAt_(0),
  volumeMl_(0),
  durationSeconds_(0),
  phaseSecondsLeft_(0),
  preinfusionMl_(0),
  preinfusionMlLeft_(0),
  phaseSeconds_{0, (uint8_t)(PREINFUSION_TIMEOUT_MS / 1000UL), 0, 0, 0},
  manual_(false),
  lossMl_(DEFAULT_LOSS_ML),
  netMl_(0.0f),
  finishedNormally_(false),
  endReason_(END_NONE),
  endPhase_(0),
  finalInMl_(0.0f),
  finalOutMl_(0.0f),
  flowCheckMl_(0.0f),
  flowCheckAt_(0),
  stopDelayS_(STOP_DELAY_DEFAULT_S),
  settlePending_(false),
  learning_(false),
  settled_(false),
  stopDelayChanged_(false),
  stopNetMl_(0.0f),
  stopNetRate_(0.0f),
  settleAt_(0)
{
}

// Pump and valve off, as at every stop.
void ExtractionController::begin() {
  pinMode(pumpPin_, OUTPUT);
  pinMode(valvePin_, OUTPUT);
  setPump(false);
  setValve(false);
}

void ExtractionController::setRefillPump(bool on) {
  if (isActive()) {
    return;
  }
  setValve(false);
  setPump(on);
}

// Clears the no-water error (the user did a long push).
void ExtractionController::clearError() {
  if (state_ == EXTRACTION_ERROR_NO_WATER) {
    state_ = EXTRACTION_IDLE;
  }
}

// Common start: refuses to start during an extraction or after a no-water error, resets the values and the
// flow sensor, takes the time of the push as the start of the duration.
bool ExtractionController::beginExtraction_(uint32_t pressTime) {
  if (isActive() || state_ == EXTRACTION_ERROR_NO_WATER) {
    return false;
  }

  const uint32_t now = millis();

  // The total duration is counted from the push of the button...
  startedAt_ = pressTime;
  // ...unless the push timestamp is not usable: start timing from now.
  if ((uint32_t)(now - startedAt_) > MAX_PRESS_TIME_AGE_MS) {
    startedAt_ = now;
  }

  flow_.startExtraction();
  settlePending_ = false;   // a new extraction ends the measure of the previous one
  learning_ = false;
  volumeMl_ = 0;
  netMl_ = 0.0f;
  finishedNormally_ = false;
  endReason_ = END_NONE;
  finalInMl_ = finalOutMl_ = 0.0f;
  durationSeconds_ = 0;
  lastVolumeAt_ = now;

  DEBUG_PRINTLN(F("Extraction started"));
  return true;
}

// AUTO extraction: pre-infusion (if its volume is not 0), then the main extraction up to the target volume.
void ExtractionController::start(uint32_t pressTime, uint16_t preinfusionMl, uint16_t waitSeconds, uint16_t flushSeconds) {
  if (!beginExtraction_(pressTime)) {
    return;
  }

  manual_ = false;
  preinfusionMl_ = preinfusionMl;
  phaseSeconds_[DURATION_WAIT] = (uint8_t)waitSeconds;
  phaseSeconds_[DURATION_FLUSH] = (uint8_t)flushSeconds;

  // The phase timers start now (the pump really starts now), not at the push time.
  enterPhase(preinfusionMl_ > 0 ? EXTRACTION_PREINFUSION : EXTRACTION_RUNNING, millis());
}

// MANUAL extraction: the pump runs until a push, or until seconds have passed if seconds is not 0.
void ExtractionController::startManual(uint32_t pressTime, uint16_t seconds) {
  if (!beginExtraction_(pressTime)) {
    return;
  }

  manual_ = true;
  phaseSeconds_[DURATION_MANUAL] = (uint8_t)seconds;
  enterPhase(EXTRACTION_MANUAL, millis());
}

// The reason why the extraction ended, with the phase it was in: the first reason is kept
void ExtractionController::endedBy(EndReason reason) {
  if (endReason_ == END_NONE) {
    endReason_ = reason;
    endPhase_ = state_;
  }
}

// Normal end of an AUTO extraction (target volume reached, or the user pushed): then the flush, if any.
void ExtractionController::finish(uint32_t now) {
  if (state_ != EXTRACTION_PREINFUSION &&
      state_ != EXTRACTION_PREINFUSION_WAIT &&
      state_ != EXTRACTION_RUNNING) {
    return;
  }

  // What the sensors count after the pump stops is measured (see settle()), for the main phase only
  const bool measureTail = (state_ == EXTRACTION_RUNNING);
  const float netAtStop = flow_.getNetVolumeLive();
  const float rateAtStop = netRateMlPerSec();

  stop(EXTRACTION_FINISHED);
  finishedNormally_ = true;

  if (measureTail) {
    settlePending_ = true;
    settled_ = false;
    settleAt_ = now;
    stopNetMl_ = netAtStop;
    stopNetRate_ = rateAtStop;
    learning_ = (rateAtStop >= STOP_LEARN_MIN_ML_PER_S);
    // REMINDER (flow_control, decision of the user 2026-11-24, may change): the stop delay is NOT learned while a flow limit
    // applies. The pump then works by slots, the flow read at the stop is noisy, and the water that comes after a stop is probably
    // partly a fixed volume (pipes), so a delay in seconds learned at a limited flow does not suit the full flow (and the other
    // way round). The delay of the normal extractions is kept as it is. To change later: learn a second delay for the limited mode
    // (another EEPROM value), or stop by a fixed volume. Locked by test/test_flow_limit_learning.cpp.
    if (flowLimitMlPerS_ > 0.0f) {
      learning_ = false;
    }
  }

  // Flush time 0: no flush, the extraction is over
  if (phaseSeconds_[DURATION_FLUSH] > 0) {
    enterPhase(EXTRACTION_FLUSH_WAIT, now);
  }
}

// The user pushed while the flush was waiting.
void ExtractionController::startFlush(uint32_t now) {
  if (state_ == EXTRACTION_FLUSH_WAIT) {
    enterPhase(EXTRACTION_FLUSHING, now);
  }
}

// Stops everything: pump first, then valve. Keeps the final volume and duration (not those of a flush).
void ExtractionController::stop(ExtractionState finalState) {
  if (!isActive()) {
    return;
  }

  // During the flush the extraction is already over: its final values are kept
  const bool flushing = isFlush();

  state_ = finalState;
  phaseSecondsLeft_ = 0;
  preinfusionMlLeft_ = 0;

  // Pump first, valve second: the pressure is released to the drip tray once the pump is
  // stopped. Closing the valve before the pump would dead-head it.
  setPump(false);
  setValve(false);

  if (!flushing) {
    flow_.stopExtraction();

    // Final values
    durationSeconds_ = (uint16_t)((uint32_t)(millis() - startedAt_) / 1000UL);
    volumeMl_ = cupVolumeMl();
    finalInMl_ = flow_.getVolumeIn();
    finalOutMl_ = flow_.getVolumeOut();
  }

  DEBUG_PRINTLN(F("Extraction stopped"));
}

uint32_t ExtractionController::phaseLimitMs() const {
  return phaseSeconds_[durationOf(state_)] * 1000UL;
}

// One pass, in this order: the flow sensor, the duration, the no-flow safety, the end of the phase (from the
// phase table), the countdowns for the screen, the volume and the target volume, the maximum duration.
void ExtractionController::update(uint32_t now, uint16_t targetVolumeMl) {
  flow_.update();
  settle(now);

  if (!isActive() || state_ == EXTRACTION_FLUSH_WAIT) {
    return;   // FLUSH_WAIT only waits for startFlush()
  }

  if (state_ == EXTRACTION_RUNNING && flowLimitMlPerS_ > 0.0f) {
    setPump(flowLimit_.tick(now, flow_.getVolumeIn()));   // the pump is a pattern of slots: the limit (see coffee/FlowLimit.h)
  }

  uint8_t flags = flagsOf(state_);
  const uint32_t elapsedSeconds = (uint32_t)(now - startedAt_) / 1000UL;

  if (!(flags & PHASE_FLUSH)) {
    // The flush comes after the extraction: its duration is not counted
    durationSeconds_ = (uint16_t)min(elapsedSeconds, (uint32_t)UINT16_MAX);
  }

  // ----- No-flow safety: the pump runs but the water does not enter the boiler -----
  if (flags & PHASE_FLOW_CHECK) {
    const float in = flow_.getVolumeIn();
    if (in - flowCheckMl_ >= NO_FLOW_MIN_ML) {
      flowCheckMl_ = in;
      flowCheckAt_ = now;
    } else if ((uint32_t)(now - flowCheckAt_) >= NO_FLOW_TIMEOUT_MS) {
      endedBy(END_NO_WATER);
      stop(EXTRACTION_ERROR_NO_WATER);
      return;
    }
  }

  // ----- End of the phase: its duration has elapsed (table), or the pre-infusion volume is reached -----
  const uint32_t limitMs = phaseLimitMs();
  bool phaseOver = (flags & PHASE_TIMED) && limitMs > 0 && (uint32_t)(now - phaseStartedAt_) >= limitMs;
  if (state_ == EXTRACTION_PREINFUSION && flow_.getNetVolume() >= (float)preinfusionMl_) {
    phaseOver = true;
  }
  if (phaseOver) {
    ExtractionState next = (ExtractionState)nextOf(state_);
    if (next == EXTRACTION_FINISHED) {
      endedBy(END_TIMER);
      stop(EXTRACTION_FINISHED);
      return;
    }
    if (next == EXTRACTION_PREINFUSION_WAIT && phaseSeconds_[DURATION_WAIT] == 0) {
      next = EXTRACTION_RUNNING;   // no soak phase if its duration is 0
    }
    enterPhase(next, now);
    flags = flagsOf(state_);
  }

  // ----- Countdowns shown on the screen (rounded up: 5, 4, 3...) -----
  // PREINFUSION: millilitres left to send. Phases with a countdown: seconds left.
  phaseSecondsLeft_ = 0;
  preinfusionMlLeft_ = 0;
  if (state_ == EXTRACTION_PREINFUSION) {
    const float left = (float)preinfusionMl_ - flow_.getNetVolume();
    preinfusionMlLeft_ = (left > 0.0f) ? (uint16_t)(left + 0.999f) : 0;
  } else if (flags & PHASE_COUNTDOWN) {
    phaseSecondsLeft_ = secondsLeft(phaseLimitMs(), now);
  }

  if (flags & PHASE_FLUSH) {
    return;   // the volume of the extraction is final
  }

  // ----- Volume in the cup, and target volume -----
  // The volume is updated in every phase (the pre-infusion water is counted), but only RUNNING ends
  // on the target volume.
  // The target is checked on every pass, with the volume counted so far (the pulses not yet converted included), and the pump
  // stops a little BEFORE it: the water that still comes after the stop fills the rest (see anticipationMl()).
  if (state_ == EXTRACTION_RUNNING && cupVolumeLiveMl() + anticipationMl() >= (float)targetVolumeMl) {
    endedBy(END_TARGET);
    finish(now);
    return;
  }

  if ((uint32_t)(now - lastVolumeAt_) >= VOLUME_PERIOD_MS) {
    lastVolumeAt_ = now;
    volumeMl_ = cupVolumeMl();
  }

  // ----- Maximum extraction duration guard -----
  if (elapsedSeconds > (uint32_t)DURATION_MAX) {
    endedBy(END_MAX_TIME);
    stop(EXTRACTION_ERROR);
  }
}

// Seconds left in a phase that started at phaseStartedAt_, rounded up; 0 once it has elapsed
uint16_t ExtractionController::secondsLeft(uint32_t phaseDurationMs, uint32_t now) const {
  const uint32_t phaseElapsedMs = (uint32_t)(now - phaseStartedAt_);
  if (phaseElapsedMs >= phaseDurationMs) {
    return 0;
  }
  return (uint16_t)((phaseDurationMs - phaseElapsedMs + 999UL) / 1000UL);
}

// Flow of the water that has to be heated, used by the heater as feedforward: In - Out, never below 0. In is the flow of the
// pump; the part that goes back through the OPV valve (Out) does not go through the boiler, so it takes no heat. With In alone
// the heater was given up to 40 % too much power in a long extraction (the OPV opens when the puck gives way: 67 ml of 172 ml on
// 2026-11-24) and the temperature went over the setpoint. The flow sensors return mL/min.
float ExtractionController::inflowMlPerSec() {
  if (!isActive()) {
    return 0.0f;
  }
  const float net = flow_.getInflowRate() - flow_.getOutflowRate();
  return (net > 0.0f ? net : 0.0f) / 60.0f;
}

// In - Out is the water that stays in the system; the part that does not end in the cup is removed.
uint16_t ExtractionController::cupVolumeMl() {
  // MANUAL mode: the water that stays in the system, nothing is removed
  netMl_ = flow_.getNetVolume();
  const float cup = netMl_ - (manual_ ? 0.0f : lossMl_);
  return (cup > 0.0f) ? (uint16_t)(cup + 0.5f) : 0;
}

// The same as cupVolumeMl() at this very moment, as a number of millilitres (not rounded, not kept at 0 when negative)
float ExtractionController::cupVolumeLiveMl() {
  return flow_.getNetVolumeLive() - (manual_ ? 0.0f : lossMl_);
}

// Flow of the water that stays in the system (In - Out), ml/s. The sensors give ml/min.
float ExtractionController::netRateMlPerSec() {
  return (flow_.getInflowRate() - flow_.getOutflowRate()) / 60.0f;
}

// The volume that still comes after the pump stops: the flow of that moment times the delay that was learned.
float ExtractionController::anticipationMl() {
  const float ml = stopDelayS_ * netRateMlPerSec();
  return constrain(ml, 0.0f, (float)STOP_ANTICIPATION_MAX_ML);
}

// Called on every pass. After the pump stopped at the end of the main phase, STOP_SETTLE_MS later the water that came after
// it is known: the delay moves toward the one that would have been right (the water that came / the flow at the stop), and the
// volume of the extraction is brought up to date (it was taken when the pump stopped, before this water).
void ExtractionController::settle(uint32_t now) {
  if (!settlePending_) {
    return;
  }
  // The pump started again (flush) or a new extraction began: the water that comes is not the one of the stop any more
  if (state_ != EXTRACTION_FLUSH_WAIT && state_ != EXTRACTION_FINISHED) {
    settlePending_ = false;
    learning_ = false;
    return;
  }
  if ((uint32_t)(now - settleAt_) < STOP_SETTLE_MS) {
    return;
  }

  settlePending_ = false;

  if (learning_) {
    learning_ = false;
    const float came = flow_.getNetVolumeLive() - stopNetMl_;                 // after the stop
    if (came >= 0.0f) {
      const float wanted = constrain(came / stopNetRate_, 0.0f, (float)STOP_DELAY_MAX_S);
      const float next = stopDelayS_ + STOP_DELAY_GAIN * (wanted - stopDelayS_);
      if (fabsf(next - stopDelayS_) >= 0.005f) {   // not worth a write of the EEPROM below
        stopDelayS_ = next;
        stopDelayChanged_ = true;
      }
    }
  }

  volumeMl_ = cupVolumeMl();   // with the water that came after the stop (also brings netMl_ up to date)
  finalInMl_ = flow_.getVolumeIn();
  finalOutMl_ = flow_.getVolumeOut();
  settled_ = true;
}

// Enter a phase: the table gives the state of the pump and of the valve. The valve is opened BEFORE the
// pump is started, so the pump never pushes against a closed valve. Stopping the pump is the opposite:
// pump first.
bool ExtractionController::switchToManual(uint32_t now, uint16_t seconds) {
  if (manual_ || (state_ != EXTRACTION_PREINFUSION && state_ != EXTRACTION_RUNNING)) {
    return false;
  }
  manual_ = true;
  phaseSeconds_[DURATION_MANUAL] = (uint8_t)seconds;
  enterPhase(EXTRACTION_MANUAL, now);   // the pump stays on (it is set on again), the valve stays open
  phaseStartedAt_ = startedAt_;         // the MANUAL time counts from the push, as the duration
  DEBUG_PRINTLN(F("Switched to MANUAL"));
  return true;
}

void ExtractionController::enterPhase(ExtractionState phase, uint32_t now) {
  state_ = phase;
  phaseStartedAt_ = now;

  const uint8_t flags = flagsOf(phase);
  const bool valveOpen = (flags & PHASE_VALVE) != 0;

  // The no-flow safety watches the pump phases: its reference restarts each time the pump starts
  if (flags & PHASE_FLOW_CHECK) {
    flowCheckMl_ = flow_.getVolumeIn();
    flowCheckAt_ = now;
  }

  if (flags & PHASE_PUMP) {
    setValve(valveOpen);
    if (phase == EXTRACTION_RUNNING && flowLimitMlPerS_ > 0.0f) {
      flowLimit_.begin(flowLimitMlPerS_, now, flow_.getVolumeIn());
    }
    setPump(true);
  } else {
    setPump(false);
    setValve(valveOpen);   // PREINFUSION_WAIT keeps it open: the pressure is held in the puck
  }

  // What the screen shows at once: the volume of the pre-infusion, the duration of a phase with a countdown
  // (in FLUSH_WAIT: what the flush will last)
  if (phase == EXTRACTION_PREINFUSION) {
    preinfusionMlLeft_ = preinfusionMl_;
  }
  if (flags & PHASE_COUNTDOWN) {
    phaseSecondsLeft_ = phaseSeconds_[durationOf(phase)];
  }

  DEBUG_PRINT(F("Phase ")); DEBUG_PRINTLN(phase);
}

// The relays are active high.
void ExtractionController::setPump(bool on) {
  digitalWrite(pumpPin_, on ? HIGH : LOW);
}

void ExtractionController::setValve(bool open) {
  digitalWrite(valvePin_, open ? HIGH : LOW);
}
