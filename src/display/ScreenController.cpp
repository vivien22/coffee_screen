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
  for (uint8_t row = 0; row < ROW_COUNT; row++) {
    fade_[row] = Fade();   // the picture replaces what was fading
  }
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
  volumeRoll_ = Roll();             // the fields are drawn as they are: no roll goes on
  durationRoll_ = Roll();
  for (uint8_t row = 0; row < ROW_COUNT; row++) {
    fade_[row] = Fade();            // nor fade
  }
  render(lastNow_);
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
  lastNow_ = now;
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
    render(now);
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

  stepFades(now);
  render(now);
  stepRolls(now);
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
  // The rows whose content changes and the letter of the drop fade, as at a change of parameter (see FADE_OUT_MS in Config.h)
  const bool animate = dropVisible_ && !steaming_ && FADE_OUT_MS + FADE_IN_MS > 0;
  finishFades();   // a letter that was fading shows its final look: the next change starts from it
  const bool wasManual = settings_.manual;
  settings_.manual = !settings_.manual;
  if (!animate) {
    drawDrop(dropVisible_, settings_.manual);
  }
  activate(PARAMETER_VOLUME, animate, animate);   // the selected parameter may not exist in the new mode
  if (animate) {
    fade_[ROW_MODE] = Fade();
    fade_[ROW_MODE].active = true;
    fade_[ROW_MODE].from.value = wasManual ? 1.0 : 0.0;
    fade_[ROW_MODE].to.value = settings_.manual ? 1.0 : 0.0;
  }
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
  activate(next, true);
}

// Selects a parameter and sets what its field shows: the setpoint while the temperature is edited, the target
// volume when the volume is selected, the live temperature again when the temperature is left.
void ScreenController::activate(ActiveParameter next, bool animate, bool modeChange) {
  // What the rows show now. A row that was fading shows its final look at once: the next change starts from it.
  RowLook before[ROW_COUNT];
  for (uint8_t row = 0; row < ROW_COUNT; row++) {
    before[row] = lookOf((Row)row, active_);
  }
  finishFades();

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

  if (!animate || FADE_OUT_MS + FADE_IN_MS == 0 || steaming_) {
    dirty_ |= DIRTY_ALL_VALUES;         // the size of the digits changes
    return;
  }

  if (modeChange) {
    // The rows as they are when the machine is ready in the new mode (showReady() does the same, just after): the fades go to them
    durationShown_ = readyDuration();
    volumeTagShown_ = PHASE_TAG_NONE;
    durationTagShown_ = PHASE_TAG_NONE;
  }

  // The rows that do not change are not touched, the ones that only change their number are drawn again, the others fade
  for (uint8_t row = 0; row < ROW_COUNT; row++) {
    const RowLook after = lookOf((Row)row, active_);
    if (after == before[row]) {
      continue;
    }
    // The row takes another look: a roll of its number in progress stops (its frames would be drawn over the new look)
    if (row == ROW_VOLUME) {
      volumeRoll_ = Roll();
    } else if (row == ROW_DURATION) {
      durationRoll_ = Roll();
    }
    if (after.sameShape(before[row]) && !modeChange) {   // at a change of mode a new number fades too
      dirty_ |= dirtyOf((Row)row);
      continue;
    }
    if (!modeChange && row == rowOf(previous) && row != rowOf(next)) {
      // The row of the parameter that was selected goes back to its normal size at once, without a fade: the eye follows the new parameter
      dirty_ |= dirtyOf((Row)row);
      if (row == ROW_TEMPERATURE) {
        temperatureErrorShown_ = false;
      }
      continue;
    }
    fade_[row] = Fade();
    fade_[row].active = true;
    fade_[row].from = before[row];
    fade_[row].to = after;
    if (row == ROW_TEMPERATURE) {
      temperatureErrorShown_ = false;   // "ERR" is drawn again when the fades are over
    }
  }
}

// Changes the selected parameter by delta, kept between its limits. Returns true if the value changed.
bool ScreenController::adjustActiveParameter(int16_t delta) {
  finishFades();   // a value is set on a row that shows its final look
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
    if (volumeRoll_.active) {                         // the roll in progress ends at once, then the new one starts
      volumeRoll_.finishFirst = true;
      volumeRoll_.finishValue = volumeShown_;
    }
    volumeShown_ = (int16_t)volumeValue;
    dirty_ |= DIRTY_VOLUME_NUMBER;
    volumeRoll_.requested = true;
  }
  if (durationTag != durationTagShown_) {
    durationShown_ = (int16_t)durationValue;
    durationTagShown_ = durationTag;
    tagBlinking_ = false;     // a new tag is drawn whole, and visible
    tagVisible_ = true;
    dirty_ |= DIRTY_DURATION;
  } else if ((int16_t)durationValue != durationShown_) {
    if (durationRoll_.active) {
      durationRoll_.finishFirst = true;
      durationRoll_.finishValue = durationShown_;
    }
    durationShown_ = (int16_t)durationValue;
    dirty_ |= DIRTY_DURATION_NUMBER;
    durationRoll_.requested = true;
  }
}

// ------------------------------------------------------------
// Drawing: only the dirty fields

// Draws the fields marked dirty. The size of the digits shows which parameter is selected (large = selected),
// and the field of a parameter being edited shows that parameter instead of the extraction values.
void ScreenController::render(uint32_t now) {
  if (dirty_ == DIRTY_NONE) {
    return;
  }

  // The rows in a fade are drawn by stepFades(), and again as they are when it is over
  const uint8_t dirty = dirty_ & ~fadingRows();
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
      volumeRoll_ = Roll();   // a setting is drawn as it is: its number does not roll
      drawVolume(settings_.preinfusion, TEXT_SIZE_LARGE, PHASE_TAG_PREINFUSION, redrawTag);
    } else if (active_ == PARAMETER_FLOW_LIMIT) {
      volumeRoll_ = Roll();
      drawFlowLimit(settings_.flowLimit, redrawTag);
    } else if (active_ == PARAMETER_CALIBRATION) {
      volumeRoll_ = Roll();
      drawVolume(calibrationValue_, TEXT_SIZE_LARGE, PHASE_TAG_CALIBRATION, redrawTag);   // the weight of the cup
    } else if (!redrawTag && volumeRoll_.requested) {
      // A new number during an extraction: it rolls. The screen shows the old one until the first frame.
      if (volumeRoll_.active && volumeRoll_.finishFirst) {
        drawVolumeValue(volumeRoll_.finishValue, false, DISPLAY_ROLL_DONE);
      }
      volumeRoll_.requested = volumeRoll_.finishFirst = false;
      volumeRoll_.active = ROLL_VOLUME_MS > 0;
      volumeRoll_.start = now;
      volumeRoll_.nextFrame = now + ROLL_FRAME_MS;
      if (!volumeRoll_.active) {
        drawVolumeValue(volumeShown_, false, DISPLAY_ROLL_DONE);
      }
    } else {
      volumeRoll_ = Roll();
      drawVolumeValue(volumeShown_, redrawTag, DISPLAY_ROLL_DONE);
    }
  }
  if (dirty & (DIRTY_DURATION | DIRTY_DURATION_NUMBER)) {
    // The tag is redrawn only if the whole field is dirty (not when only the number changed).
    const bool redrawTag = (dirty & DIRTY_DURATION) != 0;

    // While the wait or flush time is selected, the field shows that setting (large digits, with its tag).
    // Otherwise it shows the duration of the extraction (or the countdown of the wait or of the flush).
    if (active_ == PARAMETER_PREINFUSION_WAIT) {
      durationRoll_ = Roll();
      drawDuration(settings_.preinfusionWait, TEXT_SIZE_LARGE, PHASE_TAG_PREINFUSION_WAIT, redrawTag);
    } else if (active_ == PARAMETER_FLUSH) {
      durationRoll_ = Roll();
      drawDuration(settings_.flush, TEXT_SIZE_LARGE, PHASE_TAG_FLUSH, redrawTag);
    } else if (active_ == PARAMETER_MANUAL_TIME) {
      durationRoll_ = Roll();
      drawDuration(settings_.manualSeconds, TEXT_SIZE_LARGE, PHASE_TAG_NONE, redrawTag);   // no tag: the clock says it
    } else if (!redrawTag && durationRoll_.requested) {
      if (durationRoll_.active && durationRoll_.finishFirst) {
        drawDurationValue(durationRoll_.finishValue, false, DISPLAY_ROLL_DONE);
      }
      durationRoll_.requested = durationRoll_.finishFirst = false;
      durationRoll_.active = ROLL_DURATION_MS > 0;
      durationRoll_.start = now;
      durationRoll_.nextFrame = now + ROLL_FRAME_MS;
      if (!durationRoll_.active) {
        drawDurationValue(durationShown_, false, DISPLAY_ROLL_DONE);
      }
    } else {
      durationRoll_ = Roll();
      drawDurationValue(durationShown_, redrawTag, DISPLAY_ROLL_DONE);
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

// ------------------------------------------------------------
// Rolling digits

// The volume field as it shows the extraction (see render()), with the progress of a roll
void ScreenController::drawVolumeValue(int16_t value, bool redrawTag, uint8_t progress) {
  drawVolume(value, active_ == PARAMETER_VOLUME ? TEXT_SIZE_LARGE : TEXT_SIZE_SMALL, volumeTagShown_, redrawTag, progress);
}

void ScreenController::drawDurationValue(int16_t value, bool redrawTag, uint8_t progress) {
  drawDuration(value, TEXT_SIZE_SMALL, durationTagShown_, redrawTag, progress);
}

// The progress (0 to DISPLAY_ROLL_DONE - 1) of a roll that has lasted elapsed ms of duration: ease-out, fast first, slow at the end
static uint8_t rollProgress(uint32_t elapsed, uint32_t duration) {
  if (duration == 0) {
    return DISPLAY_ROLL_DONE - 1;   // no roll (ROLL_VOLUME_MS is 0): never reached, it only keeps the division safe
  }
  const uint32_t remaining = DISPLAY_ROLL_DONE - elapsed * DISPLAY_ROLL_DONE / duration;   // 255 at the start, 0 at the end
  const uint32_t cubed = remaining * remaining * remaining / (DISPLAY_ROLL_DONE * DISPLAY_ROLL_DONE);
  return (uint8_t)(DISPLAY_ROLL_DONE - cubed);
}

// Draws the next frame of each roll in progress, when it is due, and the end of a roll (the new number as it is).
void ScreenController::stepRolls(uint32_t now) {
  if (volumeRoll_.active && (int32_t)(now - volumeRoll_.nextFrame) >= 0) {
    const uint32_t elapsed = now - volumeRoll_.start;
    if (elapsed >= ROLL_VOLUME_MS) {
      volumeRoll_.active = false;
      drawVolumeValue(volumeShown_, false, DISPLAY_ROLL_DONE);
    } else {
      volumeRoll_.nextFrame = now + ROLL_FRAME_MS;
      drawVolumeValue(volumeShown_, false, rollProgress(elapsed, ROLL_VOLUME_MS));
    }
  }
  if (durationRoll_.active && (int32_t)(now - durationRoll_.nextFrame) >= 0) {
    const uint32_t elapsed = now - durationRoll_.start;
    if (elapsed >= ROLL_DURATION_MS) {
      durationRoll_.active = false;
      drawDurationValue(durationShown_, false, DISPLAY_ROLL_DONE);
    } else {
      durationRoll_.nextFrame = now + ROLL_FRAME_MS;
      drawDurationValue(durationShown_, false, rollProgress(elapsed, ROLL_DURATION_MS));
    }
  }
}

// ------------------------------------------------------------
// Change of parameter: the rows fade out, then in

// What a row shows when the parameter 'active' is selected (see render())
ScreenController::RowLook ScreenController::lookOf(Row row, ActiveParameter active) const {
  RowLook look;
  if (row == ROW_MODE) {
    look.value = settings_.manual ? 1.0 : 0.0;   // the letter: A (0) or M (1)
  } else if (row == ROW_TEMPERATURE) {
    if (temperatureError_ && active != PARAMETER_TEMPERATURE) {
      look.kind = LOOK_ERROR;
    } else {
      look.size = active == PARAMETER_TEMPERATURE ? TEXT_SIZE_LARGE : TEXT_SIZE_SMALL;
      look.value = temperatureShown_;
    }
  } else if (row == ROW_VOLUME) {
    if (active == PARAMETER_PREINFUSION) {
      look.kind = LOOK_PREINFUSION; look.size = TEXT_SIZE_LARGE; look.tag = PHASE_TAG_PREINFUSION; look.value = settings_.preinfusion;
    } else if (active == PARAMETER_FLOW_LIMIT) {
      look.kind = LOOK_FLOW_LIMIT; look.size = TEXT_SIZE_LARGE; look.tag = PHASE_TAG_LIMIT; look.value = settings_.flowLimit;
    } else if (active == PARAMETER_CALIBRATION) {
      look.kind = LOOK_CALIBRATION; look.size = TEXT_SIZE_LARGE; look.tag = PHASE_TAG_CALIBRATION; look.value = calibrationValue_;
    } else {
      look.size = active == PARAMETER_VOLUME ? TEXT_SIZE_LARGE : TEXT_SIZE_SMALL; look.tag = volumeTagShown_; look.value = volumeShown_;
    }
  } else {
    if (active == PARAMETER_PREINFUSION_WAIT) {
      look.kind = LOOK_WAIT; look.size = TEXT_SIZE_LARGE; look.tag = PHASE_TAG_PREINFUSION_WAIT; look.value = settings_.preinfusionWait;
    } else if (active == PARAMETER_FLUSH) {
      look.kind = LOOK_FLUSH; look.size = TEXT_SIZE_LARGE; look.tag = PHASE_TAG_FLUSH; look.value = settings_.flush;
    } else if (active == PARAMETER_MANUAL_TIME) {
      look.kind = LOOK_MANUAL; look.size = TEXT_SIZE_LARGE; look.value = settings_.manualSeconds;
    } else {
      look.tag = durationTagShown_; look.value = durationShown_;
    }
  }
  return look;
}

// Draws a row as the look says, whole, with its text at the brightness given. Its zone is erased first, unless erase is false (the next
// frame of a fade with the same look: the text is drawn over the last frame).
void ScreenController::drawLook(Row row, const RowLook &look, uint8_t brightness, bool erase) {
  if (row == ROW_MODE) {
    // The drop: drawn over the last frame (a frame of the fade), or whole, erased first (a fade that is cut: the other letter may be wider
    // than this one and leave its edges)
    setTextBrightness(brightness);
    if (erase) {
      drawDrop(dropVisible_, look.value != 0.0);
    } else {
      drawDropFrame(look.value != 0.0);
    }
    setTextBrightness(255);
    return;
  }
  if (erase) {
    forgetField(row);   // the whole row is drawn: the pixels on the screen are at another brightness
  } else {
    repaintField(row);
  }
  setTextBrightness(brightness);
  if (row == ROW_TEMPERATURE) {
    if (look.kind == LOOK_ERROR) {
      drawTemperatureError();
    } else {
      drawTemperature(look.value, (TextSize)look.size);
    }
  } else if (row == ROW_VOLUME) {
    if (look.kind == LOOK_FLOW_LIMIT) {
      drawFlowLimit((uint16_t)look.value, true);
    } else {
      drawVolume((uint16_t)look.value, (TextSize)look.size, (PhaseTag)look.tag, true);
    }
  } else {
    drawDuration((uint16_t)look.value, (TextSize)look.size, (PhaseTag)look.tag, true);
  }
  setTextBrightness(255);
}

ScreenController::Row ScreenController::rowOf(ActiveParameter parameter) {
  if (parameter == PARAMETER_TEMPERATURE) {
    return ROW_TEMPERATURE;
  }
  if (parameter == PARAMETER_PREINFUSION_WAIT || parameter == PARAMETER_FLUSH || parameter == PARAMETER_MANUAL_TIME) {
    return ROW_DURATION;
  }
  return ROW_VOLUME;   // the volume, the calibration, the pre-infusion, the flow limit
}

uint8_t ScreenController::dirtyOf(Row row) const {
  return row == ROW_TEMPERATURE ? DIRTY_TEMPERATURE : (row == ROW_VOLUME ? DIRTY_VOLUME : DIRTY_DURATION);
}

// The dirty flags of the rows that are in a fade
uint8_t ScreenController::fadingRows() const {
  uint8_t flags = DIRTY_NONE;
  for (uint8_t row = 0; row < ROW_COUNT; row++) {
    if (fade_[row].active && row != ROW_MODE) {
      flags |= (row == ROW_TEMPERATURE) ? DIRTY_TEMPERATURE : (row == ROW_VOLUME ? (DIRTY_VOLUME | DIRTY_VOLUME_NUMBER)
                                                                               : (DIRTY_DURATION | DIRTY_DURATION_NUMBER));
    }
  }
  return flags;
}

// The rows in a fade show what they show now (the final look), as it is
void ScreenController::finishFades() {
  for (uint8_t row = 0; row < ROW_COUNT; row++) {
    if (fade_[row].active) {
      fade_[row].active = false;
      drawLook((Row)row, lookOf((Row)row, active_), 255);
      if (row == ROW_TEMPERATURE) {
        temperatureErrorShown_ = false;
      }
    }
  }
}

// Draws the next frame of each fade in progress, when it is due; at the end of a fade the row is drawn as it is.
void ScreenController::stepFades(uint32_t now) {
  for (uint8_t row = 0; row < ROW_COUNT; row++) {
    Fade &fade = fade_[row];
    if (!fade.active) {
      continue;
    }
    if (!fade.started) {
      fade.started = true;
      fade.start = now;
      fade.nextFrame = now + FADE_FRAME_MS;
      continue;
    }
    const uint32_t elapsed = now - fade.start;
    if (elapsed >= FADE_OUT_MS + FADE_IN_MS) {
      fade.active = false;
      if (row == ROW_MODE) {
        drawLook(ROW_MODE, lookOf(ROW_MODE, active_), 255, false);   // the drop, at full brightness, over the last frame
        continue;
      }
      // The row as it is, whole, at full brightness. Drawn over the last frame, without erasing, when it is the look of that frame (an erase
      // followed by the drawing would blink at the very end of the fade); erased first when something changed meanwhile.
      if (lookOf((Row)row, active_) == fade.to) {
        repaintField(row);
      } else {
        forgetField(row);
      }
      dirty_ |= dirtyOf((Row)row);
      if (row == ROW_TEMPERATURE) {
        temperatureErrorShown_ = false;
      }
      continue;
    }
    if ((int32_t)(now - fade.nextFrame) < 0) {
      continue;
    }
    fade.nextFrame = now + FADE_FRAME_MS;

    const bool dimming = elapsed < FADE_OUT_MS;
    uint8_t level;
    if (dimming) {
      // the old row goes dark, and is dark (0) for the last frame of its phase
      const uint32_t span = FADE_OUT_MS > FADE_FRAME_MS ? FADE_OUT_MS - FADE_FRAME_MS : 1;
      level = elapsed >= span ? 0 : (uint8_t)(255 - 255 * elapsed / span);
    } else {
      // the new row comes up, ease-out: fast first, slow at the end
      const uint32_t remaining = 255 - (elapsed - FADE_OUT_MS) * 255 / FADE_IN_MS;
      const uint32_t eased = 255 - remaining * remaining * remaining / (255 * 255);   // 0 to 255
      const uint32_t start = 255 * FADE_IN_START_PERCENT / 100;
      level = (uint8_t)(start + eased * (255 - start) / 255);
    }
    const int16_t step = (int16_t)level - (int16_t)fade.level;
    if (dimming == fade.dimming && step < 8 && step > -8 && level != 0) {
      continue;   // hardly different from the last frame: nothing is drawn
    }
    // The first frame of a phase is drawn whole, erased first: in the first one of the fade the screen may not show the old look yet (a value
    // that was set just before is still waiting to be drawn), and the first of the fade in replaces the (dark) old look by a new one. The next
    // frames of a phase are drawn over the last one.
    const bool newPhase = (dimming != fade.dimming) || !fade.drawn;
    fade.drawn = true;
    fade.dimming = dimming;
    fade.level = level;
    drawLook((Row)row, dimming ? fade.from : fade.to, level, newPhase);
  }
}
