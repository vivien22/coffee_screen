#include "screen_controller.h"
#include "../config.h"
#include "display.h"

void ScreenController::begin(const Settings &settings) {
  settings_ = settings;
  active_ = PARAMETER_VOLUME;
  volumeShown_ = readyVolume();
  durationShown_ = readyDuration();
  temperatureShown_ = 0.0;
  dirty_ = DIRTY_ALL_VALUES;

  beginDisplay();
#ifdef DISPLAY_ICONS
  drawIcons();
  drawDrop(true, settings_.manual);   // with the letter of the mode
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

  // Blinking drop icon (no-water error): hidden at once, then toggled every DROP_BLINK_MS
  if (dropBlinking_) {
    if (dropBlinkPending_) {
      dropBlinkPending_ = false;
      dropVisible_ = false;
      dropToggleAt_ = now + DROP_BLINK_MS;
      drawDrop(false, settings_.manual);
    } else if ((int32_t)(now - dropToggleAt_) >= 0) {
      dropVisible_ = !dropVisible_;
      dropToggleAt_ = now + DROP_BLINK_MS;
      drawDrop(dropVisible_, settings_.manual);
    }
  }

  render();
}

void ScreenController::showNoWater(bool error) {
  if (error && !dropBlinking_) {
    dropBlinking_ = true;
    dropBlinkPending_ = true;
  } else if (!error && dropBlinking_) {
    dropBlinking_ = false;
    dropBlinkPending_ = false;
    if (!dropVisible_) {
      dropVisible_ = true;
      drawDrop(true, settings_.manual);   // the error is cleared: the drop stays on
    }
  }
}

// ------------------------------------------------------------
// User actions

void ScreenController::toggleMode() {
  settings_.manual = !settings_.manual;
  drawDrop(dropVisible_, settings_.manual);
  activate(PARAMETER_VOLUME);   // the selected parameter may not exist in the new mode
}

// The parameters that exist in the current mode: the pre-infusion, the wait and the flush are AUTO
// only; the MANUAL time is MANUAL only.
bool ScreenController::availableInMode(ActiveParameter p) const {
  if (settings_.manual) {
    return p == PARAMETER_VOLUME || p == PARAMETER_MANUAL_TIME || p == PARAMETER_TEMPERATURE;
  }
  return p != PARAMETER_MANUAL_TIME;
}

void ScreenController::selectNextParameter() {
  // AUTO: volume -> pre-infusion -> wait -> flush -> temperature -> volume
  // MANUAL: volume -> MANUAL time -> temperature -> volume
  ActiveParameter next = active_;
  do {
    next = (ActiveParameter)((next + 1) % PARAMETER_COUNT);
  } while (!availableInMode(next));
  activate(next);
}

void ScreenController::activate(ActiveParameter next) {
  const ActiveParameter previous = active_;
  active_ = next;

  if (previous == PARAMETER_TEMPERATURE) {
    temperatureShown_ = liveTemperature_;
    temperatureKey_ = -1;               // the live temperature is displayed again at once
  }
  if (active_ == PARAMETER_TEMPERATURE) {
    temperatureShown_ = settings_.temperature;   // the setpoint is shown while it is edited
  }
  if (active_ == PARAMETER_VOLUME) {
    volumeShown_ = readyVolume();                // the target volume, not the last extraction
  }
  if (previous == PARAMETER_MANUAL_TIME) {
    durationShown_ = settings_.manualSeconds;    // the time just set is the one that stays shown
  }

  dirty_ |= DIRTY_ALL_VALUES;           // the size of the digits changes
}

bool ScreenController::adjustActiveParameter(int16_t delta) {
  if (settings_.manual && active_ == PARAMETER_VOLUME) {
    return false;   // MANUAL mode: the volume is not a target, the field shows 0
  }

  int16_t *value;
  int16_t maxValue;
  uint8_t dirtyField;

  switch (active_) {
    case PARAMETER_VOLUME:
      value = &settings_.volume;          maxValue = VOLUME_MAX;              dirtyField = DIRTY_VOLUME;
      break;
    case PARAMETER_PREINFUSION:
      value = &settings_.preinfusion;     maxValue = PREINFUSION_MAX_ML;      dirtyField = DIRTY_VOLUME_NUMBER;
      break;
    case PARAMETER_PREINFUSION_WAIT:
      value = &settings_.preinfusionWait; maxValue = PREINFUSION_WAIT_MAX_SECONDS; dirtyField = DIRTY_DURATION_NUMBER;
      break;
    case PARAMETER_FLUSH:
      value = &settings_.flush;           maxValue = FLUSH_MAX_SECONDS;       dirtyField = DIRTY_DURATION_NUMBER;
      break;
    case PARAMETER_MANUAL_TIME:
      value = &settings_.manualSeconds;   maxValue = MANUAL_MAX_SECONDS;      dirtyField = DIRTY_DURATION;
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
    case PARAMETER_PREINFUSION:   // the pre-infusion volume is shown in the volume field
      clearVolume();
      blinkField_ = DIRTY_VOLUME;
      break;
    case PARAMETER_TEMPERATURE:
      clearTemperature();
      blinkField_ = DIRTY_TEMPERATURE;
      break;
    default:   // wait or flush: shown in the duration field
      clearDuration();
      blinkField_ = DIRTY_DURATION;
      break;
  }
  blinking_ = true;
  blinkEndsAt_ = now + SAVE_BLINK_MS;
}

// ------------------------------------------------------------
// Machine state to show

void ScreenController::showTemperatureError() {
  if (!temperatureError_) {
    temperatureError_ = true;
    dirty_ |= DIRTY_TEMPERATURE;
  }
}

void ScreenController::showLiveTemperature(uint32_t now, double celsius) {
  liveTemperature_ = celsius;
  if (temperatureError_) {
    temperatureError_ = false;
    temperatureKey_ = -1;               // the temperature is displayed again at once
  }

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
  volumeTagShown_ = PHASE_TAG_NONE;
  durationTagShown_ = PHASE_TAG_NONE;
  tagBlinking_ = false;
  tagVisible_ = true;
  dirty_ |= DIRTY_VOLUME | DIRTY_DURATION;
}

// The volume shown when nothing is going on: the target volume, or 0 in MANUAL mode (no target)
uint16_t ScreenController::readyVolume() const {
  return settings_.manual ? 0 : settings_.volume;
}

// The duration shown when nothing is going on: 0 s, or the MANUAL time that will be counted down
uint16_t ScreenController::readyDuration() const {
  return settings_.manual ? settings_.manualSeconds : 0;
}

void ScreenController::showReady() {
  volumeShown_ = readyVolume();   // the target volume, not the last extraction
  durationShown_ = readyDuration();
  volumeTagShown_ = PHASE_TAG_NONE;
  durationTagShown_ = PHASE_TAG_NONE;
  tagBlinking_ = false;
  tagVisible_ = true;
  dirty_ |= DIRTY_VOLUME | DIRTY_DURATION;
}

void ScreenController::showExtractionProgress(uint16_t volumeMl, uint16_t durationSeconds) {
  showExtractionValues(volumeMl, PHASE_TAG_NONE, durationSeconds, PHASE_TAG_NONE);
}

void ScreenController::showPreinfusion(uint16_t mlLeft, uint16_t durationSeconds) {
  showExtractionValues(mlLeft, PHASE_TAG_PREINFUSION, durationSeconds, PHASE_TAG_NONE);
}

void ScreenController::showPreinfusionWait(uint16_t volumeMl, uint16_t secondsLeft) {
  showExtractionValues(volumeMl, PHASE_TAG_NONE, secondsLeft, PHASE_TAG_PREINFUSION_WAIT);
}

void ScreenController::showFlush(uint16_t volumeMl, uint16_t secondsLeft, bool flushing) {
  showExtractionValues(volumeMl, PHASE_TAG_NONE, secondsLeft, PHASE_TAG_FLUSH);

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

// A whole field is redrawn when its tag changes; when only the number changes (countdown,
// elapsed time, volume), the tag is left alone.
void ScreenController::showExtractionValues(uint16_t volumeValue, PhaseTag volumeTag, uint16_t durationValue, PhaseTag durationTag) {
  if (volumeTag != volumeTagShown_) {
    volumeShown_ = (int16_t)volumeValue;
    volumeTagShown_ = volumeTag;
    dirty_ |= DIRTY_VOLUME;
  } else if ((int16_t)volumeValue != volumeShown_) {
    volumeShown_ = (int16_t)volumeValue;
    dirty_ |= DIRTY_VOLUME_NUMBER;
  }
  if (durationTag != durationTagShown_) {
    durationShown_ = (int16_t)durationValue;
    durationTagShown_ = durationTag;
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

  if ((dirty & DIRTY_TEMPERATURE) && temperatureError_ && active_ != PARAMETER_TEMPERATURE) {
    drawTemperatureError();
  } else if (dirty & DIRTY_TEMPERATURE) {
    drawTemperature(temperatureShown_, active_ == PARAMETER_TEMPERATURE ? TEXT_SIZE_LARGE : TEXT_SIZE_SMALL);
  }
  if (dirty & (DIRTY_VOLUME | DIRTY_VOLUME_NUMBER)) {
    // The tag is redrawn only if the whole field is dirty (not when only the number changed).
    const bool redrawTag = (dirty & DIRTY_VOLUME) != 0;

    // While the pre-infusion volume is selected, the field shows that setting (large digits, with its tag).
    // Otherwise it shows the volume (or the countdown of the pre-infusion volume).
    if (active_ == PARAMETER_PREINFUSION) {
      drawVolume(settings_.preinfusion, TEXT_SIZE_LARGE, PHASE_TAG_PREINFUSION, redrawTag);
    } else {
      drawVolume(volumeShown_, active_ == PARAMETER_VOLUME ? TEXT_SIZE_LARGE : TEXT_SIZE_SMALL, volumeTagShown_, redrawTag);
    }
  }
  if (dirty & (DIRTY_DURATION | DIRTY_DURATION_NUMBER)) {
    // The tag is redrawn only if the whole field is dirty (not when only the number changed).
    const bool redrawTag = (dirty & DIRTY_DURATION) != 0;

    // While the wait or flush time is selected, the field shows that setting (large digits, with its tag).
    // Otherwise it shows the duration of the extraction (or the countdown of the wait or of the flush).
    if (active_ == PARAMETER_PREINFUSION_WAIT) {
      drawDuration(settings_.preinfusionWait, TEXT_SIZE_LARGE, PHASE_TAG_PREINFUSION_WAIT, redrawTag);
    } else if (active_ == PARAMETER_FLUSH) {
      drawDuration(settings_.flush, TEXT_SIZE_LARGE, PHASE_TAG_FLUSH, redrawTag);
    } else if (active_ == PARAMETER_MANUAL_TIME) {
      drawDuration(settings_.manualSeconds, TEXT_SIZE_LARGE, PHASE_TAG_NONE, redrawTag);   // no tag: the clock says it
    } else {
      drawDuration(durationShown_, TEXT_SIZE_SMALL, durationTagShown_, redrawTag);
    }
  }
  // The blinking tag, after the duration: it also gives the right state to a tag just redrawn
  if ((dirty & DIRTY_TAG) && durationTagShown_ == PHASE_TAG_FLUSH && active_ != PARAMETER_FLUSH) {
    drawPhaseTag(durationTagShown_, tagVisible_);
  }
  if (dirty & DIRTY_HEATER) {
    drawHeaterIndicator(heaterShown_);
  }
}
