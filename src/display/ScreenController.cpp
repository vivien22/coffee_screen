// ScreenController.cpp: parameter cycle, dirty flags and the redraw rules (what is drawn, when).

#include "ScreenController.h"
#include "../Config.h"
#include "Display.h"

// Starts the screen with the saved settings: the volume is selected and every field is drawn.
void ScreenController::begin(const Settings &settings) {
  editor_.begin(settings);
  volumeShown_ = editor_.readyVolume();
  durationShown_ = editor_.readyDuration();
  temperatureShown_ = 0.0;
  temperatureErrorShown_ = false;   // the screen is filled again by beginDisplay()
  dirty_ = DIRTY_ALL_VALUES;

  beginDisplay();
  beginWelcome();                // the welcome picture is shown (the backlight goes on)
  backlight_.reset(WELCOME_MS > 0 ? 255 : 0);   // beginDisplay() switched it off, beginWelcome() on
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
  backlight_.reset(0);   // endWelcome() switched it off
#ifdef DISPLAY_ICONS
  drawIcons();
  drawDrop(true, settings().manual);   // with the letter of the mode
#endif
}

// The backlight comes up after the first drawing: the whole screen appears at once, and fades in (BACKLIGHT_FADE_IN_MS) while the loop runs.
void ScreenController::show() {
  if (BACKLIGHT_FADE_IN_MS == 0) {
    backlight_.set(255);
    return;
  }
  backlight_.fadeIn();
}

// The screen is going to be replaced (steaming, standby): the old one must not be drawn on any more, and nothing of the new one may be
// drawn while the old one is lit. The standby picture is not to be drawn over either.
bool ScreenController::canDraw() const {
#if WELCOME_MS > 0
  if (standbyShown_) {
    return false;
  }
#endif
  return backlight_.phase() != BacklightFade::OUT;
}

// A big change of the screen. The state (steaming_, standbyShown_...) is already the new one. The backlight fades out, update() draws the new
// screen when it is off (drawNewScreen: the one of the state at that moment, so a second change during the fade only changes what is drawn)
// and fades the backlight in. Without fades (both durations 0) the new screen is drawn at once.
void ScreenController::changeScreen() {
  for (uint8_t row = 0; row < ROW_COUNT; row++) {
    fade_[row] = Fade();   // the old screen goes away with what was fading and rolling
  }
  volumeRoll_ = DigitRoll();
  durationRoll_ = DigitRoll();

  if (BACKLIGHT_FADE_OUT_MS + BACKLIGHT_FADE_IN_MS == 0) {
    backlight_.set(0);
    drawNewScreen();
    backlight_.set(255);
    return;
  }
  backlight_.fadeOut();
}

// The screen as the state describes it, over a cleared screen
void ScreenController::drawNewScreen() {
#if WELCOME_MS > 0
  if (standbyShown_) {
    showStandbyPicture();
    return;
  }
#endif
  clearScreen();
  redrawScreen();
}

// The standby: the picture replaces the screen. What changes meanwhile (the temperature...) only marks the fields
// dirty: leaveStandby() draws them all again.
void ScreenController::enterStandby() {
#if WELCOME_MS > 0
  standbyShown_ = true;
  changeScreen();   // the picture replaces what was fading
#endif
}

// The end of the standby: the picture goes dark, then the icons, the drop (as it is: the no-water error
// may be blinking) and every field are drawn again in the dark, and the screen is lit: it comes back complete, as at start-up.
void ScreenController::leaveStandby() {
#if WELCOME_MS > 0
  standbyShown_ = false;
  changeScreen();
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
    drawDrop(dropBlink_.visible(), settings().manual);
  }
#endif
  dirty_ = steaming_ ? (DIRTY_TEMPERATURE | DIRTY_HEATER) : (DIRTY_ALL_VALUES | DIRTY_HEATER);
  arcShown_ = 0;                    // the ring is drawn again as it is wanted (the screen was cleared, or the ring is not on it)
  arcErase_ = false;
  temperatureErrorShown_ = false;   // ERR is gone with the rest of the screen
  temperatureKey_ = -1;             // the live temperature is drawn again at once
  volumeRoll_ = DigitRoll();             // the fields are drawn as they are: no roll goes on
  durationRoll_ = DigitRoll();
  for (uint8_t row = 0; row < ROW_COUNT; row++) {
    fade_[row] = Fade();            // nor fade
  }
  render(lastNow_);
  if (!steaming_) {
    stepArc(255);
  }
}

// Steaming mode: the screen is cleared, the temperature alone is drawn. The encoder sets the steaming temperature.
void ScreenController::enterSteaming() {
  steaming_ = true;
  steamSetpointShown_ = false;
  editor_.select(PARAMETER_TEMPERATURE);
  temperatureShown_ = liveTemperature_;
  changeScreen();
}

// Back to the brewing: the volume is selected, the real screen is drawn again in full.
void ScreenController::leaveSteaming() {
  steaming_ = false;
  activate(PARAMETER_VOLUME);
  showReady();
  changeScreen();
}

// Called on every pass: runs the blinking of the tag and of the drop, then draws what changed.
// Nothing is drawn when nothing changed, which keeps the loop fast.
void ScreenController::update(uint32_t now) {
  lastNow_ = now;
  if (backlight_.step(now)) {
    drawNewScreen();       // the backlight is off: the new screen, in the dark
    backlight_.fadeIn();
  }
  if (backlight_.phase() == BacklightFade::OUT) {
    return;             // the old screen is going dark: nothing is drawn on it
  }
#if WELCOME_MS > 0
  if (standbyShown_) {
    return;   // the picture of the standby is on the screen: nothing is drawn over it
  }
#endif

  if (steaming_) {   // the temperature alone: no tag, no drop
    if (steamSetpointShown_ && (int32_t)(now - steamSetpointUntil_) >= 0) {
      const RowLook before = lookOf(ROW_TEMPERATURE, editor_.active());
      steamSetpointShown_ = false;      // the setpoint was shown long enough: the measured temperature (or ERR) comes back
      temperatureKey_ = -1;
      temperatureShown_ = liveTemperature_;
      dirty_ |= DIRTY_TEMPERATURE;
      if (canDraw() && FADE_OUT_MS + FADE_IN_MS > 0) {
        fadeRow(ROW_TEMPERATURE, before, false);   // as a change of parameter: the setpoint fades out, the measured temperature fades in
      }
    }
    stepFades(now);
    render(now);
    return;
  }

  // Blinking tag: shown and hidden alternately, FLUSH_BLINK_MS each (not while the tag fades in: the blinking waits)
  if (tagBlink_.update(now, fade_[ROW_DURATION].active)) {
    dirty_ |= DIRTY_TAG;
  }

  // Blinking drop icon (no-water error): hidden at once, then toggled every DROP_BLINK_MS
  if (dropBlink_.update(now)) {
    drawDrop(dropBlink_.visible(), settings().manual);
  }

  stepFades(now);
  render(now);
  stepRolls(now);
  stepArc(ARC_SEGMENTS_PER_UPDATE);
}

// The segments of the arc that are missing are drawn, at most "most" of them (the loop goes on between two calls). A target smaller
// than what is shown, or none, erases the ring first.
void ScreenController::stepArc(uint8_t most) {
  if (arcErase_ || arcTarget_ < arcShown_) {
    eraseArc();
    arcShown_ = 0;
    arcErase_ = false;
  }
  if (arcTarget_ > arcShown_) {
    const uint8_t last = (arcTarget_ - arcShown_ > most) ? arcShown_ + most : arcTarget_;
    drawArcSegments(arcShown_, last);
    arcShown_ = last;
  }
}

// The arc follows the water that went through the machine (In - Out) against what the target needs: the target volume plus the loss
// (the cup holds In - Out - loss), plus the wait of the pre-infusion at ARC_WAIT_ML_PER_S. It moves from the first millilitre, even while the
// cup is still at 0, and it does not stop during the wait. AUTO only: in MANUAL there is no target.
void ScreenController::showWaterSent(float netMl, float waitSeconds) {
  // The wait of the pre-infusion counts as water at ARC_WAIT_ML_PER_S: the arc does not stop while the pump is off
  const float waitMl = settings().preinfusion > 0 ? settings().preinfusionWait * ARC_WAIT_ML_PER_S : 0.0f;
  const float needed = settings().volume + settings().loss + waitMl;
  if (settings().manual || needed < 1.0f) {
    return;
  }
  const float water = netMl + (waitMl > 0.0f ? waitSeconds * ARC_WAIT_ML_PER_S : 0.0f);
  const float segments = water <= 0.0f ? 0.0f : water * ARC_SEGMENTS / needed;
  const uint8_t target = segments >= ARC_SEGMENTS ? ARC_SEGMENTS : (uint8_t)segments;
  if (target > arcTarget_ || target + 3 < arcTarget_) {   // it only goes back for a real drop (a new extraction), not for the noise of the sensors
    arcTarget_ = target;
  }
}

// The drop icon blinks while the no-water error lasts, and stays on again once it is cleared.
void ScreenController::showNoWater(bool error) {
  if (error) {
    dropBlink_.start();
  } else if (dropBlink_.stop() && canDraw()) {
    drawDrop(true, settings().manual);   // the error is cleared: the drop stays on
  }
}

// ------------------------------------------------------------
// User actions

// AUTO <-> MANUAL: the letter in the drop changes and the volume is selected again.
void ScreenController::toggleMode() {
  // The rows whose content changes and the letter of the drop fade, as at a change of parameter (see FADE_OUT_MS in Config.h)
  const bool animate = dropBlink_.visible() && !steaming_ && canDraw() && FADE_OUT_MS + FADE_IN_MS > 0;
  finishFades();   // a letter that was fading shows its final look: the next change starts from it
  const bool wasManual = settings().manual;
  editor_.toggleMode();
  if (!animate && canDraw()) {
    drawDrop(dropBlink_.visible(), settings().manual);
  }
  activate(PARAMETER_VOLUME, animate, animate);   // the selected parameter may not exist in the new mode
  if (animate) {
    fade_[ROW_MODE] = Fade();
    fade_[ROW_MODE].active = true;
    fade_[ROW_MODE].from.value = wasManual ? 1.0 : 0.0;
    fade_[ROW_MODE].to.value = settings().manual ? 1.0 : 0.0;
  }
}

// The cup weight entered in CAL is validated (see ParameterEditor::applyCalibration()). Returns the loss.
float ScreenController::applyCalibration(float netMl) {
  if (editor_.applyCalibration(netMl)) {
    activate(PARAMETER_VOLUME);   // the calibration is done: back to the volume, not on to the next parameter
  }
  return settings().loss;
}

// The next parameter of the cycle of the double push (see ParameterEditor::next())
void ScreenController::selectNextParameter() {
  activate(editor_.next(), true);
}

// Selects a parameter and sets what its field shows: the setpoint while the temperature is edited, the target
// volume when the volume is selected, the live temperature again when the temperature is left.
void ScreenController::activate(ActiveParameter next, bool animate, bool modeChange) {
  // What the rows show now. A row that was fading shows its final look at once: the next change starts from it.
  RowLook before[ROW_COUNT];
  for (uint8_t row = 0; row < ROW_COUNT; row++) {
    before[row] = lookOf((Row)row, editor_.active());
  }
  finishFades();

  const ActiveParameter previous = editor_.active();
  editor_.select(next);

  if (previous == PARAMETER_TEMPERATURE) {
    temperatureShown_ = liveTemperature_;
    temperatureKey_ = -1;               // the live temperature is displayed again at once
  }
  if (editor_.active() == PARAMETER_TEMPERATURE) {
    temperatureShown_ = settings().temperature;   // the setpoint is shown while it is edited
  }
  if (editor_.active() == PARAMETER_VOLUME) {
    volumeShown_ = editor_.readyVolume();                // the target volume, not the last extraction
  }
  if (previous == PARAMETER_MANUAL_TIME) {
    durationShown_ = settings().manualSeconds;    // the time just set is the one that stays shown
  }

  if (!animate || FADE_OUT_MS + FADE_IN_MS == 0 || steaming_) {
    dirty_ |= DIRTY_ALL_VALUES;         // the size of the digits changes
    return;
  }

  if (modeChange) {
    // The rows as they are when the machine is ready in the new mode (showReady() does the same, just after): the fades go to them
    durationShown_ = editor_.readyDuration();
    volumeTagShown_ = PHASE_TAG_NONE;
    durationTagShown_ = PHASE_TAG_NONE;
  }

  // The rows that do not change are not touched, the ones that only change their number are drawn again, the others fade
  for (uint8_t row = 0; row < ROW_COUNT; row++) {
    const RowLook after = lookOf((Row)row, editor_.active());
    if (after == before[row]) {
      continue;
    }
    // The row takes another look: a roll of its number in progress stops (its frames would be drawn over the new look)
    if (row == ROW_VOLUME) {
      volumeRoll_ = DigitRoll();
    } else if (row == ROW_DURATION) {
      durationRoll_ = DigitRoll();
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

// Changes the selected parameter by delta, kept between its limits (ParameterEditor). Returns true if the value changed: its field
// shows the new value at once.
bool ScreenController::adjustActiveParameter(int16_t delta) {
  finishFades();   // a value is set on a row that shows its final look
  const ActiveParameter active = editor_.active();
  const RowLook before = lookOf(ROW_TEMPERATURE, active);
  if (!editor_.adjust(delta, steaming_)) {
    return false;
  }

  if (active == PARAMETER_VOLUME) {
    volumeShown_ = settings().volume;
  } else if (active == PARAMETER_TEMPERATURE) {
    temperatureShown_ = editor_.temperatureSetting(steaming_);
    if (steaming_) {
      steamSetpointShown_ = true;
      steamSetpointUntil_ = millis() + STEAM_SETPOINT_SHOW_MS;
      // The first turn: the measured temperature fades out and the setpoint fades in, as at a change of parameter (the next turns only
      // change the number, which the fade in follows)
      if (canDraw() && FADE_OUT_MS + FADE_IN_MS > 0) {
        fadeRow(ROW_TEMPERATURE, before, false);
      }
    }
  }
  dirty_ |= dirtyOfSetting(active);
  return true;
}

// The volume and the temperature are drawn whole; a setting shown with its tag (pre-infusion, limit, CAL, wait, flush) only has its number
// drawn again; the MANUAL time has no tag, its field is drawn whole.
uint8_t ScreenController::dirtyOfSetting(ActiveParameter parameter) const {
  switch (parameter) {
    case PARAMETER_VOLUME:           return DIRTY_VOLUME;
    case PARAMETER_PREINFUSION:
    case PARAMETER_FLOW_LIMIT:
    case PARAMETER_CALIBRATION:      return DIRTY_VOLUME_NUMBER;
    case PARAMETER_PREINFUSION_WAIT:
    case PARAMETER_FLUSH:            return DIRTY_DURATION_NUMBER;
    case PARAMETER_MANUAL_TIME:      return DIRTY_DURATION;
    default:                         return DIRTY_TEMPERATURE;
  }
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
  if (steaming_ ? steamSetpointShown_ : editor_.active() == PARAMETER_TEMPERATURE) {
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
  tagBlink_.stop();
  dirty_ |= DIRTY_VOLUME | DIRTY_DURATION;
  arcTarget_ = 0;   // the ring of the last extraction (if it is still there) goes, the new one starts empty
}

// Nothing is going on: the fields show the target volume (0 in MANUAL) and 0 s (the MANUAL time in MANUAL).
void ScreenController::showReady() {
  arcTarget_ = 0;                 // nothing is going on: no ring
  // The rows that change (their tag, or their number: "27 s" -> "0 s") fade, as at a change of parameter. A row that is in a fade already
  // (a change of mode) goes on with it.
  const bool animate = canAnimate();
  RowLook before[2] = {lookOf(ROW_VOLUME, editor_.active()), lookOf(ROW_DURATION, editor_.active())};
  if (tagBlink_.blinking() && !tagBlink_.visible() && before[1].tag == PHASE_TAG_FLUSH) {
    before[1].tag = PHASE_TAG_NONE;   // the tag is hidden by its blinking: nothing of it to fade out
  }

  volumeShown_ = editor_.readyVolume();   // the target volume, not the last extraction
  durationShown_ = editor_.readyDuration();
  volumeTagShown_ = PHASE_TAG_NONE;
  durationTagShown_ = PHASE_TAG_NONE;
  tagBlink_.stop();
  if (!(animate && !fade_[ROW_VOLUME].active && fadeRow(ROW_VOLUME, before[0], true))) {
    dirty_ |= DIRTY_VOLUME;
  }
  if (!(animate && !fade_[ROW_DURATION].active && fadeRow(ROW_DURATION, before[1], true))) {
    dirty_ |= DIRTY_DURATION;
  }
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
  if (flushing) {
    tagBlink_.start();
  } else if (tagBlink_.stop()) {
    dirty_ |= DIRTY_TAG;
  }
}

// A whole field is redrawn when its tag changes; when only the number changes (countdown,
// elapsed time, volume), the tag is left alone.
// A tag that appears or disappears (PRE, WAIT, FLUSH) fades like a change of parameter (the look of the row changes).
void ScreenController::showExtractionValues(uint16_t volumeValue, PhaseTag volumeTag, uint16_t durationValue, PhaseTag durationTag) {
  const bool animate = (volumeTag != volumeTagShown_ || durationTag != durationTagShown_) && canAnimate();
  RowLook before[2];
  if (animate) {
    before[0] = lookOf(ROW_VOLUME, editor_.active());
    before[1] = lookOf(ROW_DURATION, editor_.active());
    if (tagBlink_.blinking() && !tagBlink_.visible() && before[1].tag == PHASE_TAG_FLUSH) {
      before[1].tag = PHASE_TAG_NONE;   // the tag is hidden by its blinking: nothing of it to fade out
    }
    finishFades();   // a row that was fading shows its final look: the next change starts from it
  }

  if (volumeTag != volumeTagShown_) {
    volumeShown_ = (int16_t)volumeValue;
    volumeTagShown_ = volumeTag;
    if (!(animate && fadeRow(ROW_VOLUME, before[0], false))) {
      dirty_ |= DIRTY_VOLUME;
    }
  } else if ((int16_t)volumeValue != volumeShown_) {
    volumeRoll_.request(volumeShown_);                // (a roll in progress ends at once, then the new one starts)
    volumeShown_ = (int16_t)volumeValue;
    dirty_ |= DIRTY_VOLUME_NUMBER;
  }
  if (durationTag != durationTagShown_) {
    durationShown_ = (int16_t)durationValue;
    durationTagShown_ = durationTag;
    tagBlink_.stop();     // a new tag is drawn whole, and visible
    if (!(animate && fadeRow(ROW_DURATION, before[1], false))) {
      dirty_ |= DIRTY_DURATION;
    }
  } else if ((int16_t)durationValue != durationShown_) {
    durationRoll_.request(durationShown_);
    durationShown_ = (int16_t)durationValue;
    dirty_ |= DIRTY_DURATION_NUMBER;
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

  if (steaming_) {   // the temperature alone, and the heater dot
    if (dirty & DIRTY_TEMPERATURE) {
      const bool error = temperatureError_ && !steamSetpointShown_;
      drawSteamTemperature(temperatureShown_, error);
    }
    if (dirty & (DIRTY_HEATER | DIRTY_TEMPERATURE)) {   // after the temperature: its zone can erase the edge of the dot
      drawHeaterIndicator(heaterShown_, true);
    }
    return;
  }

  if ((dirty & DIRTY_TEMPERATURE) && temperatureError_ && editor_.active() != PARAMETER_TEMPERATURE) {
    // "ERR" erases its zone before it is drawn: it is drawn again only if something else was drawn there since,
    // not each time the field is marked dirty without a change (a double push to the next parameter)
    if (!temperatureErrorShown_) {
      drawTemperatureError();
      temperatureErrorShown_ = true;
    }
  } else if (dirty & DIRTY_TEMPERATURE) {
    drawTemperature(temperatureShown_, editor_.active() == PARAMETER_TEMPERATURE ? TEXT_SIZE_LARGE : TEXT_SIZE_SMALL);
    temperatureErrorShown_ = false;
  }
  if (dirty & (DIRTY_VOLUME | DIRTY_VOLUME_NUMBER)) {
    // The tag is redrawn only if the whole field is dirty (not when only the number changed).
    const bool redrawTag = (dirty & DIRTY_VOLUME) != 0;

    // While the pre-infusion volume (or the cup weight of the calibration) is selected, the field shows that
    // setting (large digits, with its tag).
    // Otherwise it shows the volume (or the countdown of the pre-infusion volume).
    if (editor_.active() == PARAMETER_PREINFUSION) {
      volumeRoll_ = DigitRoll();   // a setting is drawn as it is: its number does not roll
      drawVolume(settings().preinfusion, TEXT_SIZE_LARGE, PHASE_TAG_PREINFUSION, redrawTag);
    } else if (editor_.active() == PARAMETER_FLOW_LIMIT) {
      volumeRoll_ = DigitRoll();
      drawFlowLimit(settings().flowLimit, redrawTag);
    } else if (editor_.active() == PARAMETER_CALIBRATION) {
      volumeRoll_ = DigitRoll();
      drawVolume(editor_.calibrationValue(), TEXT_SIZE_LARGE, PHASE_TAG_CALIBRATION, redrawTag);   // the weight of the cup
    } else if (!redrawTag && volumeRoll_.requested()) {
      // A new number during an extraction: it rolls. The screen shows the old one until the first frame.
      int16_t finished;
      if (volumeRoll_.start(now, ROLL_VOLUME_MS, finished)) {
        drawVolumeValue(finished, false, DISPLAY_ROLL_DONE);
      }
      if (!volumeRoll_.active()) {
        drawVolumeValue(volumeShown_, false, DISPLAY_ROLL_DONE);
      }
    } else {
      volumeRoll_ = DigitRoll();
      drawVolumeValue(volumeShown_, redrawTag, DISPLAY_ROLL_DONE);
    }
  }
  if (dirty & (DIRTY_DURATION | DIRTY_DURATION_NUMBER)) {
    // The tag is redrawn only if the whole field is dirty (not when only the number changed).
    const bool redrawTag = (dirty & DIRTY_DURATION) != 0;

    // While the wait or flush time is selected, the field shows that setting (large digits, with its tag).
    // Otherwise it shows the duration of the extraction (or the countdown of the wait or of the flush).
    if (editor_.active() == PARAMETER_PREINFUSION_WAIT) {
      durationRoll_ = DigitRoll();
      drawDuration(settings().preinfusionWait, TEXT_SIZE_LARGE, PHASE_TAG_PREINFUSION_WAIT, redrawTag);
    } else if (editor_.active() == PARAMETER_FLUSH) {
      durationRoll_ = DigitRoll();
      drawDuration(settings().flush, TEXT_SIZE_LARGE, PHASE_TAG_FLUSH, redrawTag);
    } else if (editor_.active() == PARAMETER_MANUAL_TIME) {
      durationRoll_ = DigitRoll();
      drawDuration(settings().manualSeconds, TEXT_SIZE_LARGE, PHASE_TAG_NONE, redrawTag);   // no tag: the clock says it
    } else if (!redrawTag && durationRoll_.requested()) {
      int16_t finished;
      if (durationRoll_.start(now, ROLL_DURATION_MS, finished)) {
        drawDurationValue(finished, false, DISPLAY_ROLL_DONE);
      }
      if (!durationRoll_.active()) {
        drawDurationValue(durationShown_, false, DISPLAY_ROLL_DONE);
      }
    } else {
      durationRoll_ = DigitRoll();
      drawDurationValue(durationShown_, redrawTag, DISPLAY_ROLL_DONE);
    }
  }
  // The blinking tag, after the duration: it also gives the right state to a tag just redrawn. Not while the duration row shows a setting
  // (wait, flush, manual time): the tag of that setting, or none, is on the screen.
  if ((dirty & DIRTY_TAG) && durationTagShown_ == PHASE_TAG_FLUSH && rowOf(editor_.active()) != ROW_DURATION) {
    drawPhaseTag(durationTagShown_, tagBlink_.visible());
  }
  if (dirty & DIRTY_HEATER) {
    drawHeaterIndicator(heaterShown_);
  }
}

// ------------------------------------------------------------
// Rolling digits

// The volume field as it shows the extraction (see render()), with the progress of a roll
void ScreenController::drawVolumeValue(int16_t value, bool redrawTag, uint8_t progress) {
  drawVolume(value, editor_.active() == PARAMETER_VOLUME ? TEXT_SIZE_LARGE : TEXT_SIZE_SMALL, volumeTagShown_, redrawTag, progress);
}

void ScreenController::drawDurationValue(int16_t value, bool redrawTag, uint8_t progress) {
  drawDuration(value, TEXT_SIZE_SMALL, durationTagShown_, redrawTag, progress);
}

// Draws the next frame of each roll in progress, when it is due, and the end of a roll (the new number as it is).
void ScreenController::stepRolls(uint32_t now) {
  uint8_t progress;
  if (volumeRoll_.frame(now, ROLL_VOLUME_MS, progress)) {
    drawVolumeValue(volumeShown_, false, progress);
  }
  if (durationRoll_.frame(now, ROLL_DURATION_MS, progress)) {
    drawDurationValue(durationShown_, false, progress);
  }
}

// ------------------------------------------------------------
// Change of parameter: the rows fade out, then in

// What a row shows when the parameter 'active' is selected (see render())
ScreenController::RowLook ScreenController::lookOf(Row row, ActiveParameter active) const {
  RowLook look;
  if (row == ROW_MODE) {
    look.value = settings().manual ? 1.0 : 0.0;   // the letter: A (0) or M (1)
  } else if (row == ROW_TEMPERATURE && steaming_) {
    // The steaming mode: large digits, the setpoint being set or the measured temperature (or ERR)
    look.size = TEXT_SIZE_LARGE;
    look.kind = steamSetpointShown_ ? LOOK_STEAM_SETPOINT : (temperatureError_ ? LOOK_ERROR : LOOK_VALUE);
    look.value = temperatureShown_;
  } else if (row == ROW_TEMPERATURE) {
    if (temperatureError_ && active != PARAMETER_TEMPERATURE) {
      look.kind = LOOK_ERROR;
    } else {
      look.size = active == PARAMETER_TEMPERATURE ? TEXT_SIZE_LARGE : TEXT_SIZE_SMALL;
      look.value = temperatureShown_;
    }
  } else if (row == ROW_VOLUME) {
    if (active == PARAMETER_PREINFUSION) {
      look.kind = LOOK_PREINFUSION; look.size = TEXT_SIZE_LARGE; look.tag = PHASE_TAG_PREINFUSION; look.value = settings().preinfusion;
    } else if (active == PARAMETER_FLOW_LIMIT) {
      look.kind = LOOK_FLOW_LIMIT; look.size = TEXT_SIZE_LARGE; look.tag = PHASE_TAG_LIMIT; look.value = settings().flowLimit;
    } else if (active == PARAMETER_CALIBRATION) {
      look.kind = LOOK_CALIBRATION; look.size = TEXT_SIZE_LARGE; look.tag = PHASE_TAG_CALIBRATION; look.value = editor_.calibrationValue();
    } else {
      look.size = active == PARAMETER_VOLUME ? TEXT_SIZE_LARGE : TEXT_SIZE_SMALL; look.tag = volumeTagShown_; look.value = volumeShown_;
    }
  } else {
    if (active == PARAMETER_PREINFUSION_WAIT) {
      look.kind = LOOK_WAIT; look.size = TEXT_SIZE_LARGE; look.tag = PHASE_TAG_PREINFUSION_WAIT; look.value = settings().preinfusionWait;
    } else if (active == PARAMETER_FLUSH) {
      look.kind = LOOK_FLUSH; look.size = TEXT_SIZE_LARGE; look.tag = PHASE_TAG_FLUSH; look.value = settings().flush;
    } else if (active == PARAMETER_MANUAL_TIME) {
      look.kind = LOOK_MANUAL; look.size = TEXT_SIZE_LARGE; look.value = settings().manualSeconds;
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
      drawDrop(dropBlink_.visible(), look.value != 0.0);
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
  if (row == ROW_TEMPERATURE && steaming_) {
    drawSteamTemperature(look.value, look.kind == LOOK_ERROR);
    setTextBrightness(255);
    drawHeaterIndicator(heaterShown_, true);   // the zone of the temperature reaches the edge of the dot
    return;
  }
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

bool ScreenController::canAnimate() const {
  return !steaming_ && canDraw() && FADE_OUT_MS + FADE_IN_MS > 0;
}

bool ScreenController::fadeRow(Row row, const RowLook &before, bool numberToo) {
  const RowLook after = lookOf(row, editor_.active());
  if (numberToo ? after == before : after.sameShape(before)) {
    return false;
  }
  if (row == ROW_VOLUME) {
    volumeRoll_ = DigitRoll();   // its number is drawn as it is, by the fade
  } else if (row == ROW_DURATION) {
    durationRoll_ = DigitRoll();
  }
  fade_[row] = Fade();
  fade_[row].active = true;
  fade_[row].from = before;
  fade_[row].to = after;
  return true;
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

// A fade (of a row or of the backlight) or a roll is going on or has just been asked for: the frames come from update(), a block operation of the data flash now would
// make them jerk
bool ScreenController::animating() const {
  if (backlight_.phase() != BacklightFade::STEADY) {
    return true;   // the backlight fades at a big change of the screen
  }
  for (uint8_t row = 0; row < ROW_COUNT; row++) {
    if (fade_[row].active) {
      return true;
    }
  }
  return volumeRoll_.busy() || durationRoll_.busy();
}

// The rows in a fade show what they show now (the final look), as it is
void ScreenController::finishFades() {
  for (uint8_t row = 0; row < ROW_COUNT; row++) {
    if (fade_[row].active) {
      fade_[row].active = false;
      drawLook((Row)row, lookOf((Row)row, editor_.active()), 255);
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
        drawLook(ROW_MODE, lookOf(ROW_MODE, editor_.active()), 255, false);   // the drop, at full brightness, over the last frame
        continue;
      }
      // The row as it is, whole, at full brightness. Drawn over the last frame, without erasing, when it is the look of that frame (an erase
      // followed by the drawing would blink at the very end of the fade); erased first when something changed meanwhile.
      if (lookOf((Row)row, editor_.active()) == fade.to) {
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
    // The look that comes in shows the number as it is now (the countdown of a phase goes on while its tag fades in). A new number has
    // another width: that frame is drawn whole, erased first, like the first one of a phase.
    bool numberChanged = false;
    if (!dimming && row != ROW_MODE) {
      const RowLook now = lookOf((Row)row, editor_.active());
      if (now.sameShape(fade.to) && now.value != fade.to.value) {
        fade.to = now;
        numberChanged = true;
      }
    }
    const int16_t step = (int16_t)level - (int16_t)fade.level;
    if (dimming == fade.dimming && step < 8 && step > -8 && level != 0 && !numberChanged) {
      continue;   // hardly different from the last frame: nothing is drawn
    }
    // The first frame of a phase is drawn whole, erased first: in the first one of the fade the screen may not show the old look yet (a value
    // that was set just before is still waiting to be drawn), and the first of the fade in replaces the (dark) old look by a new one. The next
    // frames of a phase are drawn over the last one.
    const bool newPhase = (dimming != fade.dimming) || !fade.drawn || numberChanged;
    fade.drawn = true;
    fade.dimming = dimming;
    fade.level = level;
    drawLook((Row)row, dimming ? fade.from : fade.to, level, newPhase);
  }
}
