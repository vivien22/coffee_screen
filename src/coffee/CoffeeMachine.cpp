// CoffeeMachine.cpp: the main update() pass and the handlers of the encoder actions (short, double, long push).

#include "CoffeeMachine.h"
#include "../Config.h"

// screen_ is set up by begin(): its members are not given a value by the constructor
// cppcheck-suppress uninitMemberVar
CoffeeMachine::CoffeeMachine() :
  encoder_(PIN_ENCODER_GA, PIN_ENCODER_GB, LONG_PUSH_MS, DOUBLE_PUSH_MS),
  temperature_(PIN_MAX31865_CS),
  heater_(PIN_HEATER_RELAY),
  extraction_(PIN_PUMP_RELAY, PIN_VALVE_RELAY, flow_),
  wasExtracting_(false)
{
}

// Start order matters: the outputs (heater, pump, valve) are switched off first, then the saved settings are loaded
// and every module is started with them.
void CoffeeMachine::begin() {
  console_.begin();

  // Outputs first: heater, pump and valve off.
  heater_.begin();
  extraction_.begin();

  const Settings settings = store_.load();

  screen_.begin(settings);   // the welcome picture is shown from here, for WELCOME_MS

  // The sensors start while the picture is shown (the probe waits 100 ms): the screen stays dark for less time
  // afterwards. The encoder does not: a push during the picture must not start anything.
  temperature_.begin();
  flow_.begin();

  heater_.setSetpoint(settings.temperature + TEMPERATURE_OFFSET);
  extraction_.setLossMl(settings.loss);
  extraction_.setStopDelaySeconds(store_.loadStopDelay());

  if (temperature_.isValid()) {
    screen_.showLiveTemperature(millis(), temperature_.celsius() - TEMPERATURE_OFFSET);
  } else {
    screen_.showTemperatureError();
  }

  screen_.finishWelcome();   // the rest of the time of the picture, then the icons are drawn in the dark
  encoder_.begin();
#ifdef START_BUTTON
  pinMode(PIN_START_BUTTON, INPUT_PULLUP);
  buttonRaw_ = buttonDown_ = (digitalRead(PIN_START_BUTTON) == LOW);   // a button that is held at power-up does nothing
#endif
  lastActivityAt_ = millis();
  screen_.update(millis());

  // Everything is drawn (icons, mode letter, the three values): only now does the backlight come on
  screen_.show();
}

// One pass of the main loop, called as often as possible (no delay anywhere). Order of the work:
//   1. encoder: the user actions (they may start an extraction)
//   2. extraction: phases, volume, safeties
//   3. temperature and heater (heater off during standby)
//   4. what the screen shows, then the drawing of what changed
void CoffeeMachine::update() {
  console_.update(journal_);   // answers a command of the serial line (reads only)

  // ----- 1. User actions -----
  // The encoder is polled on every pass: its debounce / click state machine needs it.
  // Done before reading the clock: an action may start an extraction, whose start time
  // must not be later than "now".
  pollEncoder();
  pollStartButton();

  const uint32_t now = millis();

  // ----- 2. Extraction (not in the steaming mode: no extraction, no volume is read) -----
  if (!steaming_) {
    updateExtraction(now);
  }

  // ----- 3. Temperature, standby and heater -----
  temperature_.update(now);

#if STANDBY_MINUTES > 0
  // An extraction counts as activity (the encoder actions are counted in pollEncoder()).
  if (extraction_.isActive()) {
    lastActivityAt_ = now;
    wakeUp();
  } else if (!standby_ && (uint32_t)(now - lastActivityAt_) >= STANDBY_MINUTES * 60000UL) {
    standby_ = true;
    screen_.enterStandby();   // the welcome picture replaces the screen
  }
#endif

  const bool heating = !standby_ && temperature_.isValid();   // standby: the heater is off (same as a probe error)
  heater_.update(now,
                 heating,
                 temperature_.celsius(),
                 extraction_.inflowMlPerSec());

  // ----- 4. What the screen shows -----
  if (!steaming_) {
    showExtraction(now);
  }

  if (temperature_.isValid()) {
    screen_.showLiveTemperature(now, temperature_.celsius() - TEMPERATURE_OFFSET);
  } else {
    screen_.showTemperatureError();   // the heater is off (see HeaterController)
  }
  screen_.showHeaterState(heater_.isOn());
  screen_.update(now);
}

// Section 2 of update(): the extraction (phases, volume, safeties) and what follows from it.
void CoffeeMachine::updateExtraction(uint32_t now) {
  extraction_.update(now, screen_.settings().volume);

  // The delay of the stop before the target was learned: kept for the next extractions
  if (extraction_.stopDelayLearned()) {
    store_.saveStopDelay(extraction_.stopDelaySeconds());
  }
  // The water that came after the stop is counted: the final volume of an extraction without flush is shown again (with a
  // flush the screen already follows it)
  if (extraction_.takeSettled() && extraction_.state() == EXTRACTION_FINISHED && resultShown_) {
    screen_.showExtractionProgress(extraction_.volumeMl(), extraction_.durationSeconds());
  }

  // An extraction that ended normally can be used to calibrate the loss (CAL parameter): offered once
  if (extraction_.calibratable() && !extraction_.settlePending() && !calibrationOffered_) {
    calibrationOffered_ = true;
    screen_.offerCalibration(extraction_.volumeMl());
  }

  // The journal: the extraction is over (how it ended is known) and its volumes are final
  if (journalPending_ && extraction_.endReason() != END_NONE && !extraction_.settlePending()) {
    journalPending_ = false;
    writeJournal();
  }

  // The drop icon blinks until the no-water error is cleared
  screen_.showNoWater(extraction_.hasNoWaterError());
}

// One line in the journal of the last extractions (EEPROM, see Journal.h)
void CoffeeMachine::writeJournal() {
  const Settings &settings = screen_.settings();
  JournalEntry entry;
  entry.sequence = 0;   // given by the journal
  entry.reason = extraction_.endReason();
  entry.phase = extraction_.endPhase();
  entry.target = settings.volume;
  entry.preinfusion = settings.preinfusion;
  entry.wait = settings.preinfusionWait;
  entry.duration = (uint8_t)extraction_.durationSeconds();   // 120 s at most (DURATION_MAX)
  entry.inTenths = (uint16_t)(extraction_.volumeInMl() * 10.0f + 0.5f);
  entry.outTenths = (uint16_t)(extraction_.volumeOutMl() * 10.0f + 0.5f);
  entry.loss4 = (uint8_t)(extraction_.loss() * 4.0f + 0.5f);
  entry.cup = (uint8_t)extraction_.volumeMl();
  entry.delay100 = (uint8_t)(extraction_.stopDelaySeconds() * 100.0f + 0.5f);
  entry.tempStart = journalTempStart_;
  entry.tempEnd = (uint8_t)(int)(temperature_.celsius() - TEMPERATURE_OFFSET);
  entry.cal = 0;   // set later if the cup weight is entered in CAL
  journal_.add(entry);
}

// The weight entered in CAL is validated: written in the journal (last entry = the extraction CAL was offered for) only if it was
// changed, then the loss is corrected (the same thing for the double push and for the start button).
void CoffeeMachine::validateCalibration() {
  if (screen_.calibrationChanged()) {
    journal_.setLastCal(screen_.calibrationWeight());
  }
  extraction_.setLossMl(screen_.applyCalibration(extraction_.netVolumeMl()));
}

// Section 4 of update(): the state of the extraction on the screen; the final values of an extraction are shown once it stopped.
void CoffeeMachine::showExtraction(uint32_t now) {
  // Show the state of the machine; the final values of an extraction are shown once it stopped.
  const bool extracting = extraction_.isActive();
  const bool justStopped = wasExtracting_ && !extracting;
  const bool flushJustEnded = justStopped && (lastState_ == EXTRACTION_FLUSH_WAIT || lastState_ == EXTRACTION_FLUSHING);

  if (flushJustEnded) {
    // End of the flush: the values of the extraction are shown again, for EXTRACTION_RESULT_DISPLAY_MS (below)
    screen_.showExtractionProgress(extraction_.volumeMl(), extraction_.durationSeconds());
  } else if (extracting || wasExtracting_) {
    switch (extraction_.state()) {
      case EXTRACTION_PREINFUSION:
        // Millilitres left to send, with the "PRE" tag; the duration runs as usual
        screen_.showPreinfusion(extraction_.preinfusionMlLeft(), extraction_.durationSeconds());
        break;
      case EXTRACTION_PREINFUSION_WAIT:
        // Seconds left of the wait, with the "WAIT" tag
        screen_.showPreinfusionWait(extraction_.volumeMl(), extraction_.phaseSecondsLeft());
        break;
      case EXTRACTION_MANUAL:
        // MANUAL mode: the time counts down to 0 if a MANUAL time is set, otherwise it runs
        screen_.showExtractionProgress(extraction_.volumeMl(),
            extraction_.phaseSecondsLeft() > 0 ? extraction_.phaseSecondsLeft() : extraction_.durationSeconds());
        break;
      case EXTRACTION_FLUSH_WAIT:
        // Waiting for the push: the duration of the extraction stays on the screen, with the "FLUSH" tag
        screen_.showFlush(extraction_.volumeMl(), extraction_.durationSeconds(), false);
        break;
      case EXTRACTION_FLUSHING:
        // Countdown of the flush, with the blinking "FLUSH" tag
        screen_.showFlush(extraction_.volumeMl(), extraction_.phaseSecondsLeft(), true);
        break;
      default:
        // Main extraction, or final values of an extraction that just stopped
        screen_.showExtractionProgress(extraction_.volumeMl(), extraction_.durationSeconds());
        break;
    }
  }
  wasExtracting_ = extracting;
  if (extracting) {
    lastState_ = extraction_.state();
  }

  // An extraction that ended (without flush, or at the end of the flush, never before it): its values stay
  // EXTRACTION_RESULT_DISPLAY_MS, then the target volume and 0 s are shown. A new extraction cancels it.
  if (justStopped) {
    resultShownUntil_ = now + EXTRACTION_RESULT_DISPLAY_MS;
    resultShown_ = true;
  }
  if (resultShown_) {
    if (extracting) {
      resultShown_ = false;
    } else if ((int32_t)(now - resultShownUntil_) >= 0) {
      resultShown_ = false;
      screen_.showReady();
    }
  }
}

// ------------------------------------------------------------
// User actions

// Reads the encoder and hands each event to its handler. While the machine is in standby, the first action
// only wakes it up: it is not handled.
void CoffeeMachine::pollEncoder() {
  byte leftSteps = 0;
  byte rightSteps = 0;
  PushEvent pushEvent = PUSH_NONE;

  if (!encoder_.update(leftSteps, rightSteps, pushEvent)) {
    return;
  }

  lastActivityAt_ = millis();   // any action wakes the machine up...
  if (standby_) {
    wakeUp();
    return;                     // ...and is not handled
  }

  const int16_t delta = ((int16_t)rightSteps - (int16_t)leftSteps) * ENCODER_STEP;
  if (delta != 0) {
    adjustActiveParameter(delta);
  }

  // The encoder library emits at most one push classification per update call.
  handlePush(pushEvent);
}

// What each push of the encoder does. The long push enters and leaves the steaming mode, where the short and the double push
// do nothing (the temperature is the only thing to set).
void CoffeeMachine::handlePush(PushEvent pushEvent) {
  switch (pushEvent) {
    case PUSH_LONG:   toggleSteaming();   break;
    case PUSH_SHORT:  if (!steaming_) handleShortPush();  break;
    case PUSH_DOUBLE: if (!steaming_) handleDoublePush(); break;
    default: break;
  }
}

// The steaming mode (long push, to enter it and to leave it), only when nothing is going on: the heater regulates the steaming
// temperature, there is no extraction (it is not updated: no volume is read, the start button does nothing), and the screen shows
// the temperature alone. What was being set is validated first (and the mode of the machine stays as it is).
void CoffeeMachine::toggleSteaming() {
  if (extraction_.isActive()) {
    return;
  }
  saveSettings();
  steaming_ = !steaming_;
  if (steaming_) {
    screen_.enterSteaming();
  } else {
    screen_.leaveSteaming();
    resultShown_ = false;
  }
  saveSettings();   // the setpoint of the mode that starts
}

// The end of the standby: the heater works again and the real screen comes back (see ScreenController::leaveStandby()).
void CoffeeMachine::wakeUp() {
  if (standby_) {
    standby_ = false;
    screen_.leaveStandby();
  }
}

// A turn of the encoder changes the selected parameter. During an extraction only the temperature can be changed.
void CoffeeMachine::adjustActiveParameter(int16_t delta) {
  // Only the temperature can be changed during an extraction (flush included): the volume and
  // the times are read when it starts
  if (screen_.activeParameter() != PARAMETER_TEMPERATURE && extraction_.isActive()) {
    return;
  }
  if (screen_.adjustActiveParameter(delta) && steaming_) {
    saveSettings();   // SteamingTemp is saved at each change, and applied to the heater at once
  }
}

// What the start / stop button does while an extraction runs.
void CoffeeMachine::handleExtractionPush() {
  switch (extraction_.state()) {
    case EXTRACTION_ERROR_NO_WATER:
      // No extraction until the error is cleared by the short push of the encoder
      break;

    case EXTRACTION_PREINFUSION:
    case EXTRACTION_PREINFUSION_WAIT:
    case EXTRACTION_RUNNING:
      // The user ends the extraction: next is the flush (if its time is not 0)
      extraction_.endedBy(END_USER);
      extraction_.finish(millis());
      break;

    case EXTRACTION_MANUAL:
      // MANUAL mode: the second push stops the pump (no flush)
      extraction_.endedBy(END_USER);
      extraction_.stop(EXTRACTION_FINISHED);
      break;

    case EXTRACTION_FLUSH_WAIT:
      extraction_.startFlush(millis());
      break;

    case EXTRACTION_FLUSHING:
      // Stop the flush before its time
      extraction_.stop(EXTRACTION_FINISHED);
      break;

    default:   // IDLE, FINISHED, ERROR: the encoder does not start an extraction (the start / stop button does)
      break;
  }
}

// Starts the extraction with the settings as they are (AUTO or MANUAL), after saving them: what was just set is validated by
// the start (written only if it changed). Used by the start / stop button.
void CoffeeMachine::startExtraction(uint32_t pressTime) {
  const Settings &settings = screen_.settings();
  saveSettings();
  calibrationOffered_ = false;   // the previous extraction can no longer be used to calibrate
  screen_.withdrawCalibration();
  if (settings.manual) {
    extraction_.startManual(pressTime, settings.manualSeconds);
  } else {
#ifdef FLOW_LIMIT
    extraction_.setFlowLimit(settings.flowLimit / 10.0f);   // AUTO only: the MANUAL extraction has no target to limit toward
#endif
    extraction_.start(pressTime, settings.preinfusion, settings.preinfusionWait, settings.flush);
  }
  screen_.showExtractionStarted();
  journalTempStart_ = (uint8_t)(int)(temperature_.celsius() - TEMPERATURE_OFFSET);   // for the journal
  journalPending_ = extraction_.isActive() && !settings.manual;   // the journal gets this extraction (AUTO only) when its values are final
}

// The start / stop button, read on every pass: the level has to stay START_BUTTON_DEBOUNCE_MS, and the action is taken at the
// push (not at the release). Like any action it wakes the machine up, and that first action is not handled.
void CoffeeMachine::pollStartButton() {
#ifdef START_BUTTON
  const uint32_t now = millis();
  const bool pushed = (digitalRead(PIN_START_BUTTON) == LOW);
  if (pushed != buttonRaw_) {
    buttonRaw_ = pushed;
    buttonChangedAt_ = now;
  }
  if (buttonRaw_ != buttonDown_ && (uint32_t)(now - buttonChangedAt_) >= START_BUTTON_DEBOUNCE_MS) {
    buttonDown_ = buttonRaw_;
    if (buttonDown_) {
      lastActivityAt_ = now;
      if (standby_) {
        wakeUp();
      } else if (!steaming_) {   // the steaming mode has no extraction
        handleStartButton();
      }
    }
  }
#endif
}

// The push of the start / stop button:
//   an extraction is running (or its flush): the push of the encoder (end it, start / stop the flush, stop MANUAL);
//   after a no-water error: nothing (the long push of the encoder clears it);
//   nothing is running: whatever was being set is kept, the volume is selected again and an extraction starts.
void CoffeeMachine::handleStartButton() {
  switch (extraction_.state()) {
    case EXTRACTION_ERROR_NO_WATER:
      return;

    case EXTRACTION_IDLE:
    case EXTRACTION_FINISHED:
    case EXTRACTION_ERROR:
      break;

    default:
      handleExtractionPush();
      return;
  }

  // The weight that was being entered for CAL is validated like the double push does (the loss is corrected); every other
  // value is already in the settings, startExtraction() saves them.
  if (screen_.activeParameter() == PARAMETER_CALIBRATION) {
    validateCalibration();
  }
  screen_.selectVolume();
  startExtraction(millis());
}

// Double push: validates the parameter being edited and selects the next one (the cycle is in ScreenController.h).
// Ignored during an extraction; during a flush it stops the flush first. Validating CAL applies the calibration.
void CoffeeMachine::handleDoublePush() {
  switch (extraction_.state()) {
    case EXTRACTION_ERROR_NO_WATER:
      return;   // ignored until the error is cleared by a short push of the encoder

    case EXTRACTION_PREINFUSION:
    case EXTRACTION_PREINFUSION_WAIT:
    case EXTRACTION_RUNNING:
    case EXTRACTION_MANUAL:
      // Ignored during the extraction (pre-infusion included): a short push stops it
      return;

    case EXTRACTION_FLUSH_WAIT:
    case EXTRACTION_FLUSHING:
      // The extraction is over: the flush is skipped / stopped, then the next parameter is selected
      extraction_.stop(EXTRACTION_FINISHED);
      break;

    default:
      break;
  }
  if (screen_.activeParameter() == PARAMETER_CALIBRATION) {
    // The cup weight entered corrects the loss, used by the next extractions
    validateCalibration();
    if (screen_.activeParameter() == PARAMETER_VOLUME) {
      saveSettings();
      return;   // a weight was entered: back to the volume, the rest of the cycle is skipped
    }
  }
  saveSettings();   // the parameter that was edited is validated (written only if it changed)
  screen_.selectNextParameter();
}

// Save the settings (only the bytes that changed are written) and apply the temperature to the heater
void CoffeeMachine::saveSettings() {
  const Settings &settings = screen_.settings();
  store_.save(settings);
  heater_.setSetpoint((steaming_ ? settings.steamingTemperature : settings.temperature) + TEMPERATURE_OFFSET);
}

// Short push of the encoder: clears the no-water error, or switches between AUTO and MANUAL when nothing is going on.
void CoffeeMachine::handleShortPush() {
  // The short push clears the no-water error (the only way to clear it)
  if (extraction_.hasNoWaterError()) {
    extraction_.clearError();
    return;
  }

  // AUTO <-> MANUAL, only when nothing is going on (no extraction, pre-infusion and flush included)
  if (extraction_.isActive()) {
    return;
  }

  saveSettings();            // a parameter that was being edited is validated too
  screen_.toggleMode();
  saveSettings();            // the new mode is saved at every change

  // The values of the last extraction are reset at once, as after EXTRACTION_RESULT_DISPLAY_MS
  resultShown_ = false;
  screen_.showReady();
}
