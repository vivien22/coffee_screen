#include "extraction_controller.h"
#include "config.h"
#include "debug.h"

ExtractionController::ExtractionController(uint8_t pumpPin, USN_FlowSensor &flow) :
  pumpPin_(pumpPin),
  flow_(flow),
  state_(EXTRACTION_IDLE),
  startedAt_(0),
  lastVolumeAt_(0),
  volumeMl_(0),
  durationSeconds_(0)
{
}

void ExtractionController::begin() {
  pinMode(pumpPin_, OUTPUT);
  setPump(false);
}

void ExtractionController::start(uint32_t pressTime) {
  if (state_ == EXTRACTION_RUNNING) {
    return;
  }

  const uint32_t now = millis();

  state_ = EXTRACTION_RUNNING;
  startedAt_ = pressTime;
  // If the push timestamp is not usable, start timing from now.
  if ((uint32_t)(now - startedAt_) > MAX_PRESS_TIME_AGE_MS) {
    startedAt_ = now;
  }

  flow_.startExtraction();
  volumeMl_ = 0;
  durationSeconds_ = 0;
  lastVolumeAt_ = now;

  setPump(true);
  DEBUG_PRINTLN(F("Extraction started"));
}

void ExtractionController::stop(ExtractionState finalState) {
  if (state_ != EXTRACTION_RUNNING) {
    return;
  }

  state_ = finalState;
  setPump(false);
  flow_.stopExtraction();

  // Final values
  durationSeconds_ = (uint16_t)((uint32_t)(millis() - startedAt_) / 1000UL);
  volumeMl_ = (uint16_t)(flow_.getOutflowVolume() + 0.5f);

  DEBUG_PRINTLN(F("Extraction stopped"));
}

void ExtractionController::update(uint32_t now, uint16_t targetVolumeMl) {
  flow_.update();

  if (state_ != EXTRACTION_RUNNING) {
    return;
  }

  const uint32_t elapsedSeconds = (uint32_t)(now - startedAt_) / 1000UL;
  durationSeconds_ = (uint16_t)min(elapsedSeconds, (uint32_t)UINT16_MAX);

  if ((uint32_t)(now - lastVolumeAt_) >= VOLUME_PERIOD_MS) {
    lastVolumeAt_ = now;
    volumeMl_ = (uint16_t)(flow_.getOutflowVolume() + 0.5f);

    if (volumeMl_ >= targetVolumeMl) {
      stop(EXTRACTION_FINISHED);
      return;
    }
  }

  // Maximum extraction duration guard.
  if (elapsedSeconds > (uint32_t)DURATION_MAX) {
    stop(EXTRACTION_ERROR);
  }
}

float ExtractionController::outflowMlPerSec() {
  // USN_FlowSensor returns mL/min
  return isRunning() ? flow_.getOutflowRate() / 60.0f : 0.0f;
}

void ExtractionController::setPump(bool on) {
  digitalWrite(pumpPin_, on ? HIGH : LOW);
}
