// coffee_machine.cpp: the main update() pass and the handlers of the encoder actions (short, double, long push).

#include "coffee_machine.h"
#include "../config.h"

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
  // Outputs first: heater, pump and valve off.
  heater_.begin();
  extraction_.begin();

  const Settings settings = store_.load();

  screen_.begin(settings);
  encoder_.begin();
  temperature_.begin();
  flow_.begin();

  heater_.setSetpoint(settings.temperature + TEMPERATURE_OFFSET);
  extraction_.setLossMl(settings.loss);
  lastActivityAt_ = millis();

  if (temperature_.isValid()) {
    screen_.showLiveTemperature(millis(), temperature_.celsius() - TEMPERATURE_OFFSET);
  } else {
    screen_.showTemperatureError();
  }
  screen_.update(millis());
}

// One pass of the main loop, called as often as possible (no delay anywhere). Order of the work:
//   1. encoder: the user actions (they may start an extraction)
//   2. extraction: phases, volume, safeties
//   3. temperature and heater (heater off during standby)
//   4. what the screen shows, then the drawing of what changed
void CoffeeMachine::update() {
  // ----- 1. User actions -----
  // The encoder is polled on every pass: its debounce / click state machine needs it.
  // Done before reading the clock: an action may start an extraction, whose start time
  // must not be later than "now".
  pollEncoder();

  const uint32_t now = millis();

  // ----- 2. Extraction -----
  extraction_.update(now, screen_.settings().volume);

  // An extraction that ended normally can be used to calibrate the loss (CAL parameter): offered once
  if (extraction_.calibratable() && !calibrationOffered_) {
    calibrationOffered_ = true;
    screen_.offerCalibration(extraction_.volumeMl());
  }

  // The drop icon blinks until the no-water error is cleared
  screen_.showNoWater(extraction_.hasNoWaterError());

  // ----- 3. Temperature, standby and heater -----
  temperature_.update(now);

#if STANDBY_MINUTES > 0
  // An extraction counts as activity (the encoder actions are counted in pollEncoder()).
  if (extraction_.isActive()) {
    lastActivityAt_ = now;
    standby_ = false;
  } else if (!standby_ && (uint32_t)(now - lastActivityAt_) >= STANDBY_MINUTES * 60000UL) {
    standby_ = true;
  }
#endif

  heater_.update(now,
                 !standby_ && temperature_.isValid(),   // standby: the heater is off (same as a probe error)
                 temperature_.celsius(),
                 extraction_.inflowMlPerSec());

  // ----- 4. What the screen shows -----
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
  // The last seconds of the display of the result: the values blink, they are about to be replaced
  screen_.blinkResult(now, resultShown_ && (int32_t)(resultShownUntil_ - now) <= (int32_t)RESULT_BLINK_WINDOW_MS);

  if (temperature_.isValid()) {
    screen_.showLiveTemperature(now, temperature_.celsius() - TEMPERATURE_OFFSET);
  } else {
    screen_.showTemperatureError();   // the heater is off (see HeaterController)
  }
  screen_.showHeaterState(heater_.isOn());
  screen_.update(now);
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
    standby_ = false;
    return;                     // ...and is not handled
  }

  const int16_t delta = ((int16_t)rightSteps - (int16_t)leftSteps) * ENCODER_STEP;
  if (delta != 0) {
    adjustActiveParameter(delta);
  }

  // The encoder library emits at most one push classification per update call.
  switch (pushEvent) {
    case PUSH_SHORT:  handleShortPush();  break;
    case PUSH_DOUBLE: handleDoublePush(); break;
    case PUSH_LONG:   handleLongPush();   break;
    default: break;
  }
}

// A turn of the encoder changes the selected parameter. During an extraction only the temperature can be changed.
void CoffeeMachine::adjustActiveParameter(int16_t delta) {
  // Only the temperature can be changed during an extraction (flush included): the volume and
  // the times are read when it starts
  if (screen_.activeParameter() != PARAMETER_TEMPERATURE && extraction_.isActive()) {
    return;
  }
  screen_.adjustActiveParameter(delta);
}

// Short push. What it does depends on the extraction state:
//   idle      -> starts an extraction (only when the volume is selected, so that a push while editing another
//                parameter does not start the pump by mistake)
//   running   -> ends it (AUTO: next is the flush, MANUAL: the pump stops)
//   flush     -> starts / stops the flush
void CoffeeMachine::handleShortPush() {
  switch (extraction_.state()) {
    case EXTRACTION_ERROR_NO_WATER:
      // No extraction until the error is cleared by a long push
      break;

    case EXTRACTION_PREINFUSION:
    case EXTRACTION_PREINFUSION_WAIT:
    case EXTRACTION_RUNNING:
      // The user ends the extraction: next is the flush (if its time is not 0)
      extraction_.finish(millis());
      break;

    case EXTRACTION_MANUAL:
      // MANUAL mode: the second push stops the pump (no flush)
      extraction_.stop(EXTRACTION_FINISHED);
      break;

    case EXTRACTION_FLUSH_WAIT:
      extraction_.startFlush(millis());
      break;

    case EXTRACTION_FLUSHING:
      // Stop the flush before its time
      extraction_.stop(EXTRACTION_FINISHED);
      break;

    default:   // IDLE, FINISHED, ERROR
      if (screen_.activeParameter() == PARAMETER_VOLUME) {
        const Settings &settings = screen_.settings();
        saveSettings();   // the volume that was just set is validated by the start (written only if it changed)
        calibrationOffered_ = false;   // the previous extraction can no longer be used to calibrate
        screen_.withdrawCalibration();
        if (settings.manual) {
          extraction_.startManual(encoder_.eventPressTime(), settings.manualSeconds);
        } else {
          extraction_.start(encoder_.eventPressTime(),
                            settings.preinfusion, settings.preinfusionWait, settings.flush);
        }
        screen_.showExtractionStarted();
      }
      break;
  }
}

// Double push: validates the parameter being edited and selects the next one (the cycle is in screen_controller.h).
// Ignored during an extraction; during a flush it stops the flush first. Validating CAL applies the calibration.
void CoffeeMachine::handleDoublePush() {
  switch (extraction_.state()) {
    case EXTRACTION_ERROR_NO_WATER:
      return;   // ignored until the error is cleared by a long push

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
    extraction_.setLossMl(screen_.applyCalibration(extraction_.netVolumeMl()));
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
  heater_.setSetpoint(settings.temperature + TEMPERATURE_OFFSET);
}

// Long push: clears the no-water error, or switches between AUTO and MANUAL when nothing is going on.
void CoffeeMachine::handleLongPush() {
  // A long push clears the no-water error
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
