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
#ifdef DISPLAY_ICONS
  drawIcons();
#endif
}

void ScreenController::update(uint32_t now) {
  if (blinking_ && (int32_t)(now - blinkEndsAt_) >= 0) {
    blinking_ = false;
    dirty_ |= blinkField_;
  }

  // Blinking tag: shown and hidden alternately, FLUSH_BLINK_MS each
  if (tagBlinking_) {
    if (tagBlinkPending_) {
      tagBlinkPending_ = false;
      tagToggleAt_ = now + FLUSH_BLINK_MS;
    } else if ((int32_t)(now - tagToggleAt_) >= 0) {
      tagVisible_ = !tagVisible_;
      tagToggleAt_ = now + FLUSH_BLINK_MS;
      dirty_ |= DIRTY_TAG;
    }
  }

  render();
}

// ------------------------------------------------------------
// User actions

void ScreenController::selectNextParameter() {
  // Volume -> pre-infusion -> wait -> flush -> temperature -> volume
  const ActiveParameter previous = active_;
  active_ = (ActiveParameter)((active_ + 1) % PARAMETER_COUNT);

  if (previous == PARAMETER_TEMPERATURE) {
    temperatureShown_ = liveTemperature_;
    temperatureKey_ = -1;               // the live temperature is displayed again at once
  }
  if (active_ == PARAMETER_TEMPERATURE) {
    temperatureShown_ = settings_.temperature;   // the setpoint is shown while it is edited
  }
  if (active_ == PARAMETER_VOLUME) {
    volumeShown_ = settings_.volume;             // the target volume, not the last extraction
  }

  dirty_ |= DIRTY_ALL_VALUES;           // the size of the digits changes
}

bool ScreenController::adjustActiveParameter(int16_t delta) {
  int16_t *value;
  int16_t maxValue;
  uint8_t dirtyField;

  switch (active_) {
    case PARAMETER_VOLUME:
      value = &settings_.volume;          maxValue = VOLUME_MAX;              dirtyField = DIRTY_VOLUME;
      break;
    case PARAMETER_PREINFUSION:
      value = &settings_.preinfusion;     maxValue = PREINFUSION_MAX_SECONDS; dirtyField = DIRTY_DURATION_NUMBER;
      break;
    case PARAMETER_PREINFUSION_WAIT:
      value = &settings_.preinfusionWait; maxValue = PREINFUSION_MAX_SECONDS; dirtyField = DIRTY_DURATION_NUMBER;
      break;
    case PARAMETER_FLUSH:
      value = &settings_.flush;           maxValue = FLUSH_MAX_SECONDS;       dirtyField = DIRTY_DURATION_NUMBER;
      break;
    default:
      value = &settings_.temperature;     maxValue = TEMPERATURE_MAX;         dirtyField = DIRTY_TEMPERATURE;
      break;
  }

  const int16_t newValue = constrain(*value + delta, VALUE_MIN, maxValue);
  if (newValue == *value) {
    return false;
  }
  *value = newValue;

  // The field shows the new value at once
  if (active_ == PARAMETER_VOLUME) {
    volumeShown_ = newValue;
  } else if (active_ == PARAMETER_TEMPERATURE) {
    temperatureShown_ = newValue;
  }
  dirty_ |= dirtyField;
  return true;
}

void ScreenController::confirmSave(uint32_t now) {
  switch (active_) {
    case PARAMETER_VOLUME:
      clearVolume();
      blinkField_ = DIRTY_VOLUME;
      break;
    case PARAMETER_TEMPERATURE:
      clearTemperature();
      blinkField_ = DIRTY_TEMPERATURE;
      break;
    default:   // pre-infusion, wait or flush: all are shown in the duration field
      clearDuration();
      blinkField_ = DIRTY_DURATION;
      break;
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
  showExtractionValues(volumeMl, durationSeconds, PHASE_TAG_NONE);
}

void ScreenController::showPreinfusion(uint16_t volumeMl, uint16_t secondsLeft, bool waiting) {
  showExtractionValues(volumeMl, secondsLeft, waiting ? PHASE_TAG_PREINFUSION_WAIT : PHASE_TAG_PREINFUSION);
}

void ScreenController::showFlush(uint16_t volumeMl, uint16_t secondsLeft, bool flushing) {
  showExtractionValues(volumeMl, secondsLeft, PHASE_TAG_FLUSH);

  // The tag blinks while flushing; it is shown again (and stops blinking) otherwise
  if (flushing && !tagBlinking_) {
    tagBlinking_ = true;
    tagBlinkPending_ = true;
  } else if (!flushing && tagBlinking_) {
    tagBlinking_ = false;
    if (!tagVisible_) {
      tagVisible_ = true;
      dirty_ |= DIRTY_TAG;
    }
  }
}

// The whole duration field is redrawn when its tag changes; when only the number changes
// (countdown, elapsed time), the tag is left alone.
void ScreenController::showExtractionValues(uint16_t volumeMl, uint16_t durationValue, PhaseTag tag) {
  if ((int16_t)volumeMl != volumeShown_) {
    volumeShown_ = (int16_t)volumeMl;
    dirty_ |= DIRTY_VOLUME;
  }
  if (tag != tagShown_) {
    durationShown_ = (int16_t)durationValue;
    tagShown_ = tag;
    tagBlinking_ = false;     // a new tag is drawn whole, and visible
    tagVisible_ = true;
    dirty_ |= DIRTY_DURATION;
  } else if ((int16_t)durationValue != durationShown_) {
    durationShown_ = (int16_t)durationValue;
    dirty_ |= DIRTY_DURATION_NUMBER;
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
  if (dirty & (DIRTY_DURATION | DIRTY_DURATION_NUMBER)) {
    // The tag is redrawn only if the whole field is dirty (not when only the number changed).
    const bool redrawTag = (dirty & DIRTY_DURATION) != 0;

    // While a pre-infusion, wait or flush time is selected, the field shows that setting (large digits, with its tag).
    // Otherwise it shows the duration of the extraction (or the countdown of its pre-infusion).
    if (active_ == PARAMETER_PREINFUSION) {
      drawDuration(settings_.preinfusion, TEXT_SIZE_LARGE, PHASE_TAG_PREINFUSION, redrawTag);
    } else if (active_ == PARAMETER_PREINFUSION_WAIT) {
      drawDuration(settings_.preinfusionWait, TEXT_SIZE_LARGE, PHASE_TAG_PREINFUSION_WAIT, redrawTag);
    } else if (active_ == PARAMETER_FLUSH) {
      drawDuration(settings_.flush, TEXT_SIZE_LARGE, PHASE_TAG_FLUSH, redrawTag);
    } else {
      drawDuration(durationShown_, TEXT_SIZE_SMALL, tagShown_, redrawTag);
    }
  }
  // The blinking tag, after the duration: it also gives the right state to a tag just redrawn
  if ((dirty & DIRTY_TAG) && tagShown_ == PHASE_TAG_FLUSH && active_ != PARAMETER_FLUSH) {
    drawPhaseTag(tagShown_, tagVisible_);
  }
  if (dirty & DIRTY_HEATER) {
    drawHeaterIndicator(heaterShown_);
  }
}
