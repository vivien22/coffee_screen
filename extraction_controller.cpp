#include "extraction_controller.h"
#include "config.h"
#include "debug.h"

ExtractionController::ExtractionController(uint8_t pumpPin, uint8_t valvePin, USN_FlowSensor &flow) :
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
  preinfusionMs_(0),
  waitMs_(0)
{
}

void ExtractionController::begin() {
  pinMode(pumpPin_, OUTPUT);
  pinMode(valvePin_, OUTPUT);
  setPump(false);
  setValve(false);
}

void ExtractionController::start(uint32_t pressTime, uint16_t preinfusionSeconds, uint16_t waitSeconds) {
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

  preinfusionMs_ = (uint16_t)(preinfusionSeconds * 1000UL);
  waitMs_ = (uint16_t)(waitSeconds * 1000UL);

  flow_.startExtraction();
  volumeMl_ = 0;
  durationSeconds_ = 0;
  lastVolumeAt_ = now;

  DEBUG_PRINTLN(F("Extraction started"));

  // The pre-infusion timers start now (the pump really starts now), not at the push time.
  enterPhase(preinfusionMs_ > 0 ? EXTRACTION_PREINFUSION : EXTRACTION_RUNNING, now);
}

void ExtractionController::stop(ExtractionState finalState) {
  if (!isActive()) {
    return;
  }

  state_ = finalState;
  phaseSecondsLeft_ = 0;

  // Pump first, valve second: the pressure is released to the drip tray once the pump is
  // stopped. Closing the valve before the pump would dead-head it.
  setPump(false);
  setValve(false);
  flow_.stopExtraction();

  // Final values
  durationSeconds_ = (uint16_t)((uint32_t)(millis() - startedAt_) / 1000UL);
  volumeMl_ = (uint16_t)(flow_.getOutflowVolume() + 0.5f);

  DEBUG_PRINTLN(F("Extraction stopped"));
}

void ExtractionController::update(uint32_t now, uint16_t targetVolumeMl) {
  flow_.update();

  if (!isActive()) {
    return;
  }

  const uint32_t elapsedSeconds = (uint32_t)(now - startedAt_) / 1000UL;
  durationSeconds_ = (uint16_t)min(elapsedSeconds, (uint32_t)UINT16_MAX);

  // ----- Phase sequencing: PREINFUSION -> PREINFUSION_WAIT -> RUNNING -----
  const uint32_t phaseElapsedMs = (uint32_t)(now - phaseStartedAt_);

  if (state_ == EXTRACTION_PREINFUSION && phaseElapsedMs >= preinfusionMs_) {
    // Skip the soak phase if its duration is 0
    enterPhase(waitMs_ > 0 ? EXTRACTION_PREINFUSION_WAIT : EXTRACTION_RUNNING, now);
  } else if (state_ == EXTRACTION_PREINFUSION_WAIT && phaseElapsedMs >= waitMs_) {
    enterPhase(EXTRACTION_RUNNING, now);
  }

  // ----- Countdown shown on the screen during the pre-infusion (rounded up: 5, 4, 3...) -----
  phaseSecondsLeft_ = 0;
  if (state_ == EXTRACTION_PREINFUSION || state_ == EXTRACTION_PREINFUSION_WAIT) {
    const uint32_t phaseDurationMs = (state_ == EXTRACTION_PREINFUSION) ? preinfusionMs_ : waitMs_;
    const uint32_t phaseElapsed = (uint32_t)(now - phaseStartedAt_);
    if (phaseElapsed < phaseDurationMs) {
      phaseSecondsLeft_ = (uint16_t)((phaseDurationMs - phaseElapsed + 999UL) / 1000UL);
    }
  }

  // ----- Target volume (checked in every phase: the pre-infusion water is counted) -----
  if ((uint32_t)(now - lastVolumeAt_) >= VOLUME_PERIOD_MS) {
    lastVolumeAt_ = now;
    volumeMl_ = (uint16_t)(flow_.getOutflowVolume() + 0.5f);

    if (volumeMl_ >= targetVolumeMl) {
      stop(EXTRACTION_FINISHED);
      return;
    }
  }

  // ----- Maximum extraction duration guard -----
  if (elapsedSeconds > (uint32_t)DURATION_MAX) {
    stop(EXTRACTION_ERROR);
  }
}

float ExtractionController::outflowMlPerSec() {
  // USN_FlowSensor returns mL/min
  return isActive() ? flow_.getOutflowRate() / 60.0f : 0.0f;
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
