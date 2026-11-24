// ScreenController.cpp: parameter cycle, dirty flags and the redraw rules (what is drawn, when).

#include "ScreenController.h"
#include "../Config.h"
#include "Display.h"

// Starts the screen with the saved settings: the volume is selected and every field is drawn.
void ScreenController::begin(const Settings &settings) {
  settings_ = settings;
  active_ = PARAMETER_VOLUME;
  volumeShown_ = readyVolume();
  durationShown_ = readyDuration();
  temperatureShown_ = 0.0;
  temperatureErrorShown_ = false;   // the screen is filled again by beginDisplay()
  dirty_ = DIRTY_ALL_VALUES;

  beginDisplay();
  beginWelcome();                // the welcome picture is shown (the backlight goes on)
#if WELCOME_MS > 0
  welcomeStartedAt_ = millis();
#endif
}

// The end of the welcome picture: waits for what is left of WELCOME_MS (the caller has started the sensors
// meanwhile), then the picture is cleared in the dark and the fixed part of the real screen is drawn: the icons and the
// letter of the mode. The values are drawn by the next update() and the backlight comes on with show().
void ScreenController::finishWelcome() {
#if WELCOME_MS > 0
  const uint32_t shownFor = millis() - welcomeStartedAt_;
  if (shownFor < WELCOME_MS) {
    delay(WELCOME_MS - shownFor);
  }
#endif
  endWelcome();
#ifdef DISPLAY_ICONS
  drawIcons();
  drawDrop(true, settings_.manual);   // with the letter of the mode
#endif
}

// The backlight goes on after the first drawing: the whole screen appears at once.
void ScreenController::show() {
  showDisplay();
}

// The standby: the picture replaces the screen. What changes meanwhile (the temperature...) only marks the fields
// dirty: leaveStandby() draws them all again.
void ScreenController::enterStandby() {
#if WELCOME_MS > 0
  standbyShown_ = true;
  showStandbyPicture();
#endif
}

// The end of the standby: the picture is cleared in the dark, then the icons, the drop (as it is: the no-water error
// may be blinking) and every field are drawn again, and the screen is lit: it comes back complete, as at start-up.
void ScreenController::leaveStandby() {
#if WELCOME_MS > 0
  standbyShown_ = false;
  endWelcome();
  redrawScreen();
  showDisplay();
#endif
}

// The screen in full: the icons, the drop (as it is: the no-water error may be blinking) and every field, or the steaming
// temperature alone.
void ScreenController::redrawScreen() {
#ifdef DISPLAY_ICONS
  if (steaming_) {
    drawSteamIcon();   // the thermometer stays
  } else {
    drawIcons();
    drawDrop(dropVisible_, settings_.manual);
  }
#endif
  dirty_ = steaming_ ? DIRTY_TEMPERATURE : (DIRTY_ALL_VALUES | DIRTY_HEATER);
  temperatureErrorShown_ = false;   // ERR is gone with the rest of the screen
  temperatureKey_ = -1;             // the live temperature is drawn again at once
  render();
}

// Steaming mode: the screen is cleared, the temperature alone is drawn. The encoder sets the steaming temperature.
void ScreenController::enterSteaming() {
  steaming_ = true;
  steamSetpointShown_ = false;
  active_ = PARAMETER_TEMPERATURE;
  temperatureShown_ = liveTemperature_;
  clearScreen();
  redrawScreen();
}

// Back to the brewing: the volume is selected, the real screen is drawn again in full.
void ScreenController::leaveSteaming() {
  steaming_ = false;
  activate(PARAMETER_VOLUME);
  showReady();
  clearScreen();
  redrawScreen();
}

// Called on every pass: runs the blinking of the tag and of the drop, then draws what changed.
// Nothing is drawn when nothing changed, which keeps the loop fast.
void ScreenController::update(uint32_t now) {
#if WELCOME_MS > 0
  if (standbyShown_) {
    return;   // the picture of the standby is on the screen: nothing is drawn over it
  }
#endif

  if (steaming_) {   // the temperature alone: no tag, no drop
    if (steamSetpointShown_ && (int32_t)(now - steamSetpointUntil_) >= 0) {
      steamSetpointShown_ = false;      // the setpoint was shown long enough: the measured temperature (or ERR) comes back
      temperatureKey_ = -1;
      temperatureShown_ = liveTemperature_;
      dirty_ |= DIRTY_TEMPERATURE;
    }
    render();
    return;
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

// The drop icon blinks while the no-water error lasts, and stays on again once it is cleared.
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

// AUTO <-> MANUAL: the letter in the drop changes and the volume is selected again.
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
  return p != PARAMETER_MANUAL_TIME && (p != PARAMETER_CALIBRATION || calibrationAvailable_);
}

// An AUTO extraction just ended: CAL joins the cycle, starting at the volume the machine estimated.
void ScreenController::offerCalibration(uint16_t estimateMl) {
  calibrationAvailable_ = true;
  calibrationValue_ = calibrationStart_ = estimateMl;
}

// A new extraction starts: the previous one can no longer be used to calibrate.
void ScreenController::withdrawCalibration() {
  calibrationAvailable_ = false;
}

// The cup weight entered in CAL is validated. netMl is In - Out of the extraction. The loss moves towards the
// measured loss by CALIBRATION_GAIN (see Config.h) and is kept between 0 and LOSS_MAX_ML. If the weight was
// not changed nothing is corrected. CAL leaves the cycle either way. Returns the loss.
float ScreenController::applyCalibration(float netMl) {
  if (calibrationValue_ != calibrationStart_) {
    const float loss = settings_.loss + CALIBRATION_GAIN * (netMl - calibrationValue_ - settings_.loss);
    settings_.loss = constrain(loss, 0.0f, LOSS_MAX_ML);
    activate(PARAMETER_VOLUME);   // the calibration is done: back to the volume, not on to the next parameter
  }
  calibrationAvailable_ = false;
  return settings_.loss;
}

void ScreenController::selectNextParameter() {
  // AUTO:   volume -> [CAL, only after an extraction] -> pre-infusion -> wait -> flush -> temperature -> volume
  // MANUAL: volume -> MANUAL time -> temperature -> volume
  // The loop skips the parameters that do not exist in the current mode.
  ActiveParameter next = active_;
  do {
    next = (ActiveParameter)((next + 1) % PARAMETER_COUNT);
  } while (!availableInMode(next));
  activate(next);
}

// Selects a parameter and sets what its field shows: the setpoint while the temperature is edited, the target
// volume when the volume is selected, the live temperature again when the temperature is left.
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

// Changes the selected parameter by delta, kept between its limits. Returns true if the value changed.
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
    case PARAMETER_FLOW_LIMIT:
      value = &settings_.flowLimit;       maxValue = FLOW_LIMIT_MAX_TENTHS;   dirtyField = DIRTY_VOLUME_NUMBER;
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
    case PARAMETER_CALIBRATION:
      value = &calibrationValue_;         maxValue = VOLUME_MAX;              dirtyField = DIRTY_VOLUME_NUMBER;
      break;
    default:
      value = steaming_ ? &settings_.steamingTemperature : &settings_.temperature;
      maxValue = TEMPERATURE_MAX;         dirtyField = DIRTY_TEMPERATURE;
      break;
  }

  int16_t newValue = constrain(*value + delta, VALUE_MIN, maxValue);
  // The flow limit is 0 (off) or from FLOW_LIMIT_MIN_TENTHS to the maximum: no value in between
  if (active_ == PARAMETER_FLOW_LIMIT && newValue > 0 && newValue < FLOW_LIMIT_MIN_TENTHS) {
    newValue = (delta > 0) ? FLOW_LIMIT_MIN_TENTHS : 0;
  }
  if (newValue == *value) {
    return false;
  }
  *value = newValue;

  // The field shows the new value at once
  if (active_ == PARAMETER_VOLUME) {
    volumeShown_ = newValue;
  } else if (active_ == PARAMETER_TEMPERATURE) {
    temperatureShown_ = newValue;
    if (steaming_) {
      steamSetpointShown_ = true;
      steamSetpointUntil_ = millis() + STEAM_SETPOINT_SHOW_MS;
    }
  }
  dirty_ |= dirtyField;
  return true;
}

// ------------------------------------------------------------
// Machine state to show

// The probe is faulty: "ERR" replaces the temperature.
void ScreenController::showTemperatureError() {
  if (!temperatureError_) {
    temperatureError_ = true;
    dirty_ |= DIRTY_TEMPERATURE;
  }
}

// The measured temperature (water temperature, offset already removed). Refreshed at most every
// TEMPERATURE_DISPLAY_MS, and redrawn only if the displayed text changes.
void ScreenController::showLiveTemperature(uint32_t now, double celsius) {
  liveTemperature_ = celsius;
  if (temperatureError_) {
    temperatureError_ = false;
    temperatureKey_ = -1;               // the temperature is displayed again at once
  }

  // While the temperature setpoint is edited, the setpoint is shown instead (steaming: for STEAM_SETPOINT_SHOW_MS after the
  // last turn)
  if (steaming_ ? steamSetpointShown_ : active_ == PARAMETER_TEMPERATURE) {
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

// The heater indicator is redrawn only when the heater switches.
void ScreenController::showHeaterState(bool on) {
  if (heaterKnown_ && on == heaterShown_) {
    return;
  }
  heaterKnown_ = true;
  heaterShown_ = on;
  dirty_ |= DIRTY_HEATER;
}

// An extraction starts: volume and duration start again at 0, without tag.
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

// Nothing is going on: the fields show the target volume (0 in MANUAL) and 0 s (the MANUAL time in MANUAL).
void ScreenController::showReady() {
  volumeShown_ = readyVolume();   // the target volume, not the last extraction
  durationShown_ = readyDuration();
  volumeTagShown_ = PHASE_TAG_NONE;
  durationTagShown_ = PHASE_TAG_NONE;
  tagBlinking_ = false;
  tagVisible_ = true;
  dirty_ |= DIRTY_VOLUME | DIRTY_DURATION;
}

// The next four functions give the screen the values of the phase in progress, each with its tag.
// Main extraction (or the final values of a finished one): volume and elapsed time, no tag.
void ScreenController::showExtractionProgress(uint16_t volumeMl, uint16_t durationSeconds) {
  showExtractionValues(volumeMl, PHASE_TAG_NONE, durationSeconds, PHASE_TAG_NONE);
}

// Pre-infusion: the millilitres left to send, tag "PRE".
void ScreenController::showPreinfusion(uint16_t mlLeft, uint16_t durationSeconds) {
  showExtractionValues(mlLeft, PHASE_TAG_PREINFUSION, durationSeconds, PHASE_TAG_NONE);
}

// Pre-infusion wait: the seconds left, tag "WAIT".
void ScreenController::showPreinfusionWait(uint16_t volumeMl, uint16_t secondsLeft) {
  showExtractionValues(volumeMl, PHASE_TAG_NONE, secondsLeft, PHASE_TAG_PREINFUSION_WAIT);
}

// Flush: the tag "FLUSH" is fixed while waiting for the push, and blinks while flushing.
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

// Draws the fields marked dirty. The size of the digits shows which parameter is selected (large = selected),
// and the field of a parameter being edited shows that parameter instead of the extraction values.
void ScreenController::render() {
  if (dirty_ == DIRTY_NONE) {
    return;
  }

  const uint8_t dirty = dirty_;
  dirty_ = DIRTY_NONE;

  if (steaming_) {   // the temperature alone
    if (dirty & DIRTY_TEMPERATURE) {
      const bool error = temperatureError_ && !steamSetpointShown_;
      drawSteamTemperature(temperatureShown_, error);
    }
    return;
  }

  if ((dirty & DIRTY_TEMPERATURE) && temperatureError_ && active_ != PARAMETER_TEMPERATURE) {
    // "ERR" erases its zone before it is drawn: it is drawn again only if something else was drawn there since,
    // not each time the field is marked dirty without a change (a double push to the next parameter)
    if (!temperatureErrorShown_) {
      drawTemperatureError();
      temperatureErrorShown_ = true;
    }
  } else if (dirty & DIRTY_TEMPERATURE) {
    drawTemperature(temperatureShown_, active_ == PARAMETER_TEMPERATURE ? TEXT_SIZE_LARGE : TEXT_SIZE_SMALL);
    temperatureErrorShown_ = false;
  }
  if (dirty & (DIRTY_VOLUME | DIRTY_VOLUME_NUMBER)) {
    // The tag is redrawn only if the whole field is dirty (not when only the number changed).
    const bool redrawTag = (dirty & DIRTY_VOLUME) != 0;

    // While the pre-infusion volume (or the cup weight of the calibration) is selected, the field shows that
    // setting (large digits, with its tag).
    // Otherwise it shows the volume (or the countdown of the pre-infusion volume).
    if (active_ == PARAMETER_PREINFUSION) {
      drawVolume(settings_.preinfusion, TEXT_SIZE_LARGE, PHASE_TAG_PREINFUSION, redrawTag);
    } else if (active_ == PARAMETER_FLOW_LIMIT) {
      drawFlowLimit(settings_.flowLimit, redrawTag);
    } else if (active_ == PARAMETER_CALIBRATION) {
      drawVolume(calibrationValue_, TEXT_SIZE_LARGE, PHASE_TAG_CALIBRATION, redrawTag);   // the weight of the cup
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
