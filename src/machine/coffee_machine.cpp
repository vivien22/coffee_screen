#include "coffee_machine.h"
#include "../config.h"

CoffeeMachine::CoffeeMachine() :
  encoder_(PIN_ENCODER_GA, PIN_ENCODER_GB, LONG_PUSH_MS, DOUBLE_PUSH_MS),
  temperature_(PIN_MAX31865_CS),
  heater_(PIN_HEATER_RELAY),
  extraction_(PIN_PUMP_RELAY, PIN_VALVE_RELAY, flow_),
  wasExtracting_(false)
{
}

void CoffeeMachine::begin() {
  // Outputs first: heater, pump and valve off.
  heater_.begin();
  extraction_.begin();

  const Settings settings = store_.load();

  screen_.begin(settings);
  encoder_.begin();
  temperature_.begin();
  flow_.begin();

  heater_.setSetpoint(settings.temperature);

  if (temperature_.isValid()) {
    screen_.showLiveTemperature(millis(), temperature_.celsius());
  }
  screen_.update(millis());
}

void CoffeeMachine::update() {
  // Poll the encoder on every pass; its debounce/click state machine needs this.
  // Done before reading the clock: an action may start an extraction, whose start time
  // must not be later than "now".
  pollEncoder();

  const uint32_t now = millis();

  extraction_.update(now, screen_.settings().volume);
  temperature_.update(now);
  heater_.update(now,
                 temperature_.isValid(),
                 temperature_.celsius(),
                 extraction_.inflowMlPerSec());

  // Show the state of the machine; the final values of an extraction are shown once it stopped.
  const bool extracting = extraction_.isActive();
  const bool justStopped = wasExtracting_ && !extracting;
  const bool flushJustEnded = justStopped && (lastState_ == EXTRACTION_FLUSH_WAIT || lastState_ == EXTRACTION_FLUSHING);

  if (flushJustEnded) {
    // End of the flush: the screen is ready for the next extraction at once
    screen_.showReady();
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

  // An extraction that ended without flush: its values stay EXTRACTION_RESULT_DISPLAY_MS, then the target volume
  // and 0 s are shown. A new extraction cancels it.
  if (justStopped && !flushJustEnded) {
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

  if (temperature_.isValid()) {
    screen_.showLiveTemperature(now, temperature_.celsius());
  }
  screen_.showHeaterState(heater_.isOn());
  screen_.update(now);
}

// ------------------------------------------------------------
// User actions

void CoffeeMachine::pollEncoder() {
  byte leftSteps = 0;
  byte rightSteps = 0;
  PushEvent pushEvent = PUSH_NONE;

  if (!encoder_.update(leftSteps, rightSteps, pushEvent)) {
    return;
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

void CoffeeMachine::adjustActiveParameter(int16_t delta) {
  // Only the temperature can be changed during an extraction (flush included): the volume and
  // the times are read when it starts
  if (screen_.activeParameter() != PARAMETER_TEMPERATURE && extraction_.isActive()) {
    return;
  }
  screen_.adjustActiveParameter(delta);
}

void CoffeeMachine::handleShortPush() {
  switch (extraction_.state()) {
    case EXTRACTION_PREINFUSION:
    case EXTRACTION_PREINFUSION_WAIT:
    case EXTRACTION_RUNNING:
      // The user ends the extraction: next is the flush (if its time is not 0)
      extraction_.finish(millis());
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
        extraction_.start(encoder_.eventPressTime(),
                          settings.preinfusion, settings.preinfusionWait, settings.flush);
        screen_.showExtractionStarted();
      }
      break;
  }
}

void CoffeeMachine::handleDoublePush() {
  switch (extraction_.state()) {
    case EXTRACTION_PREINFUSION:
    case EXTRACTION_PREINFUSION_WAIT:
    case EXTRACTION_RUNNING:
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
  screen_.selectNextParameter();
}

void CoffeeMachine::handleLongPush() {
  // A long push is ignored during an extraction (any phase, pre-infusion and flush included):
  // nothing is saved and the saved-value blink is not shown.
  if (extraction_.isActive()) {
    return;
  }

  const Settings &settings = screen_.settings();

  switch (screen_.activeParameter()) {
    case PARAMETER_VOLUME:
      store_.saveVolume(settings.volume);
      break;
    case PARAMETER_PREINFUSION:
      store_.savePreinfusion(settings.preinfusion);
      break;
    case PARAMETER_PREINFUSION_WAIT:
      store_.savePreinfusionWait(settings.preinfusionWait);
      break;
    case PARAMETER_FLUSH:
      store_.saveFlush(settings.flush);
      break;
    default:
      store_.saveTemperature(settings.temperature);
      heater_.setSetpoint(settings.temperature);
      break;
  }

  screen_.confirmSave(millis());
}
