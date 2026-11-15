#include "extraction_controller.h"
#include "../config.h"
#include "../debug.h"

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
  waitMs_(0),
  flushMs_(0)
{
}

void ExtractionController::begin() {
  pinMode(pumpPin_, OUTPUT);
  pinMode(valvePin_, OUTPUT);
  setPump(false);
  setValve(false);
}

void ExtractionController::start(uint32_t pressTime, uint16_t preinfusionMl, uint16_t waitSeconds, uint16_t flushSeconds) {
  if (isActive()) {
    return;
  }

  const uint32_t now = millis();

  // The total duration is counted from the push of the button...
  startedAt_ = pressTime;
  // ...unless the push timestamp is not usable: start timing from now.
  if ((uint32_t)(now - startedAt_) > MAX_PRESS_TIME_AGE_MS) {
    startedAt_ = now;
  }

  preinfusionMl_ = preinfusionMl;
  waitMs_ = (uint16_t)(waitSeconds * 1000UL);
  flushMs_ = (uint16_t)(flushSeconds * 1000UL);

  flow_.startExtraction();
  volumeMl_ = 0;
  durationSeconds_ = 0;
  lastVolumeAt_ = now;

  DEBUG_PRINTLN(F("Extraction started"));

  // The phase timers start now (the pump really starts now), not at the push time.
  enterPhase(preinfusionMl_ > 0 ? EXTRACTION_PREINFUSION : EXTRACTION_RUNNING, now);
}

void ExtractionController::finish(uint32_t now) {
  if (state_ != EXTRACTION_PREINFUSION &&
      state_ != EXTRACTION_PREINFUSION_WAIT &&
      state_ != EXTRACTION_RUNNING) {
    return;
  }

  stop(EXTRACTION_FINISHED);

  // Flush time 0: no flush, the extraction is over
  if (flushMs_ > 0) {
    enterPhase(EXTRACTION_FLUSH_WAIT, now);
  }
}

void ExtractionController::startFlush(uint32_t now) {
  if (state_ == EXTRACTION_FLUSH_WAIT) {
    enterPhase(EXTRACTION_FLUSHING, now);
  }
}

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
  }

  DEBUG_PRINTLN(F("Extraction stopped"));
}

void ExtractionController::update(uint32_t now, uint16_t targetVolumeMl) {
  flow_.update();

  if (!isActive()) {
    return;
  }

  const uint32_t phaseElapsedMs = (uint32_t)(now - phaseStartedAt_);

  // ----- Flush: the extraction is over, its volume and duration are not updated any more -----
  if (state_ == EXTRACTION_FLUSH_WAIT) {
    return;   // waits for startFlush()
  }
  if (state_ == EXTRACTION_FLUSHING) {
    if (phaseElapsedMs >= flushMs_) {
      stop(EXTRACTION_FINISHED);
    } else {
      phaseSecondsLeft_ = secondsLeft(flushMs_, now);
    }
    return;
  }

  const uint32_t elapsedSeconds = (uint32_t)(now - startedAt_) / 1000UL;
  durationSeconds_ = (uint16_t)min(elapsedSeconds, (uint32_t)UINT16_MAX);

  // ----- Phase sequencing: PREINFUSION -> PREINFUSION_WAIT -> RUNNING -----
  // PREINFUSION ends on the volume that entered the boiler (In - Out), with a time limit as a safety.
  if (state_ == EXTRACTION_PREINFUSION &&
      (flow_.getNetVolume() >= (float)preinfusionMl_ || phaseElapsedMs >= PREINFUSION_TIMEOUT_MS)) {
    // Skip the soak phase if its duration is 0
    enterPhase(waitMs_ > 0 ? EXTRACTION_PREINFUSION_WAIT : EXTRACTION_RUNNING, now);
  } else if (state_ == EXTRACTION_PREINFUSION_WAIT && phaseElapsedMs >= waitMs_) {
    enterPhase(EXTRACTION_RUNNING, now);
  }

  // ----- Countdowns shown on the screen (rounded up: 5, 4, 3...) -----
  // PREINFUSION: millilitres left to send. PREINFUSION_WAIT: seconds left.
  phaseSecondsLeft_ = 0;
  preinfusionMlLeft_ = 0;
  if (state_ == EXTRACTION_PREINFUSION) {
    const float left = (float)preinfusionMl_ - flow_.getNetVolume();
    preinfusionMlLeft_ = (left > 0.0f) ? (uint16_t)(left + 0.999f) : 0;
  } else if (state_ == EXTRACTION_PREINFUSION_WAIT) {
    phaseSecondsLeft_ = secondsLeft(waitMs_, now);
  }

  // ----- Volume in the cup, and target volume -----
  // The volume is updated in every phase (the pre-infusion water is counted), but only RUNNING ends
  // on the target volume.
  if ((uint32_t)(now - lastVolumeAt_) >= VOLUME_PERIOD_MS) {
    lastVolumeAt_ = now;
    volumeMl_ = cupVolumeMl();

    if (state_ == EXTRACTION_RUNNING && volumeMl_ >= targetVolumeMl) {
      finish(now);
      return;
    }
  }

  // ----- Maximum extraction duration guard -----
  if (elapsedSeconds > (uint32_t)DURATION_MAX) {
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

float ExtractionController::inflowMlPerSec() {
  // The flow sensor returns mL/min
  return isActive() ? flow_.getInflowRate() / 60.0f : 0.0f;
}

// In - Out is the water that stays in the system; the part that does not end in the cup is removed.
uint16_t ExtractionController::cupVolumeMl() {
  const float cup = flow_.getNetVolume() - (PUCK_RETAINED_ML + BACKFLUSH_ML);
  return (cup > 0.0f) ? (uint16_t)(cup + 0.5f) : 0;
}

// Set the actuators of a phase. The valve is opened BEFORE the pump is started, so the pump
// never pushes against a closed valve. Stopping the pump is the opposite: pump first.
void ExtractionController::enterPhase(ExtractionState phase, uint32_t now) {
  state_ = phase;
  phaseStartedAt_ = now;

  switch (phase) {
    case EXTRACTION_PREINFUSION:
      DEBUG_PRINTLN(F("Pre-infusion: pump on, valve open"));
      setValve(true);
      setPump(true);
      preinfusionMlLeft_ = preinfusionMl_;
      break;

    case EXTRACTION_PREINFUSION_WAIT:
      DEBUG_PRINTLN(F("Pre-infusion wait: pump off, valve open"));
      setPump(false);   // the valve stays open: the pressure is held in the puck
      break;

    case EXTRACTION_RUNNING:
      DEBUG_PRINTLN(F("Extraction running: pump on, valve open"));
      setValve(true);   // already open after a pre-infusion; needed if the pre-infusion is skipped
      setPump(true);
      break;

    case EXTRACTION_FLUSH_WAIT:
      DEBUG_PRINTLN(F("Flush wait: pump off, valve closed"));
      setPump(false);
      setValve(false);
      phaseSecondsLeft_ = (uint16_t)(flushMs_ / 1000UL);   // the screen shows what the flush will last
      break;

    case EXTRACTION_FLUSHING:
      DEBUG_PRINTLN(F("Flushing: pump on, valve open"));
      setValve(true);
      setPump(true);
      phaseSecondsLeft_ = secondsLeft(flushMs_, now);
      break;

    default:
      break;           // IDLE / FINISHED / ERROR are entered by stop()
  }
}

void ExtractionController::setPump(bool on) {
  digitalWrite(pumpPin_, on ? HIGH : LOW);
}

void ExtractionController::setValve(bool open) {
  digitalWrite(valvePin_, open ? HIGH : LOW);
}
