#include "screen_controller.h"
#include "config.h"
#include "display.h"

void ScreenController::begin(const Settings &settings) {
  settings_ = settings;
  active_ = PARAMETER_VOLUME;
  volumeShown_ = settings_.volume;
  durationShown_ = 0;
  temperatureShown_ = 0.0;
  dirty_ = DIRTY_ALL_VALUES;

  beginDisplay();
  drawIcons();
}

void ScreenController::update(uint32_t now) {
  if (blinking_ && (int32_t)(now - blinkEndsAt_) >= 0) {
    blinking_ = false;
    dirty_ |= blinkField_;
  }
  render();
}

// ------------------------------------------------------------
// User actions

void ScreenController::selectNextParameter() {
  if (active_ == PARAMETER_VOLUME) {
    active_ = PARAMETER_TEMPERATURE;
    temperatureShown_ = settings_.temperature;
  } else {
    active_ = PARAMETER_VOLUME;
    volumeShown_ = settings_.volume;
    temperatureShown_ = liveTemperature_;
    temperatureKey_ = -1;               // the live temperature is displayed again at once
  }

  dirty_ |= DIRTY_ALL_VALUES;           // the size of the digits changes
}

bool ScreenController::adjustActiveParameter(int16_t delta) {
  if (active_ == PARAMETER_VOLUME) {
    const int16_t value = constrain(settings_.volume + delta, VALUE_MIN, VOLUME_MAX);
    if (value == settings_.volume) {
      return false;
    }
    settings_.volume = value;
    volumeShown_ = value;
    dirty_ |= DIRTY_VOLUME;
  } else {
    const int16_t value = constrain(settings_.temperature + delta, VALUE_MIN, TEMPERATURE_MAX);
    if (value == settings_.temperature) {
      return false;
    }
    settings_.temperature = value;
    temperatureShown_ = value;
    dirty_ |= DIRTY_TEMPERATURE;
  }
  return true;
}

void ScreenController::confirmSave(uint32_t now) {
  if (active_ == PARAMETER_VOLUME) {
    clearVolume();
    blinkField_ = DIRTY_VOLUME;
  } else {
    clearTemperature();
    blinkField_ = DIRTY_TEMPERATURE;
  }
  blinking_ = true;
  blinkEndsAt_ = now + SAVE_BLINK_MS;
}

// ------------------------------------------------------------
// Machine state to show

void ScreenController::showLiveTemperature(uint32_t now, double celsius) {
  liveTemperature_ = celsius;

  // While the temperature setpoint is edited, the setpoint is shown instead
  if (active_ == PARAMETER_TEMPERATURE) {
    return;
  }

  if (temperatureKey_ >= 0 && (uint32_t)(now - lastTemperatureAt_) < TEMPERATURE_DISPLAY_MS) {
    return;
  }
  lastTemperatureAt_ = now;

  // One decimal below 100 degrees, integer above: redraw only when the displayed text changes
  const int16_t key = (celsius < 100.0)
    ? (int16_t)(celsius * 10.0 + 0.5)
    : (int16_t)((int)(celsius + 0.5) * 10);

  if (key != temperatureKey_) {
    temperatureKey_ = key;
    temperatureShown_ = celsius;
    dirty_ |= DIRTY_TEMPERATURE;
  }
}

void ScreenController::showHeaterState(bool on) {
  if (heaterKnown_ && on == heaterShown_) {
    return;
  }
  heaterKnown_ = true;
  heaterShown_ = on;
  dirty_ |= DIRTY_HEATER;
}

void ScreenController::showExtractionStarted() {
  volumeShown_ = 0;
  durationShown_ = 0;
  dirty_ |= DIRTY_VOLUME | DIRTY_DURATION;
}

void ScreenController::showExtractionProgress(uint16_t volumeMl, uint16_t durationSeconds) {
  if ((int16_t)volumeMl != volumeShown_) {
    volumeShown_ = (int16_t)volumeMl;
    dirty_ |= DIRTY_VOLUME;
  }
  if ((int16_t)durationSeconds != durationShown_) {
    durationShown_ = (int16_t)durationSeconds;
    dirty_ |= DIRTY_DURATION;
  }
}

// ------------------------------------------------------------
// Drawing: only the dirty fields

void ScreenController::render() {
  if (dirty_ == DIRTY_NONE) {
    return;
  }

  const uint8_t dirty = dirty_;
  dirty_ = DIRTY_NONE;

  if (dirty & DIRTY_TEMPERATURE) {
    drawTemperature(temperatureShown_, active_ == PARAMETER_TEMPERATURE ? TEXT_SIZE_LARGE : TEXT_SIZE_SMALL);
  }
  if (dirty & DIRTY_VOLUME) {
    drawVolume(volumeShown_, active_ == PARAMETER_VOLUME ? TEXT_SIZE_LARGE : TEXT_SIZE_SMALL);
  }
  if (dirty & DIRTY_DURATION) {
    drawDuration(durationShown_, TEXT_SIZE_SMALL);
  }
  if (dirty & DIRTY_HEATER) {
    drawHeaterIndicator(heaterShown_);
  }
}
