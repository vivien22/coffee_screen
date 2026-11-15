#include "coffee_machine.h"
#include "config.h"

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
                 extraction_.outflowMlPerSec());

  // Show the state of the machine; the final values of an extraction are shown once it stopped.
  const bool extracting = extraction_.isActive();
  if (extracting || wasExtracting_) {
    switch (extraction_.state()) {
      case EXTRACTION_PREINFUSION:
      case EXTRACTION_PREINFUSION_WAIT:
        // Countdown of the phase, with the "PRE" / "WAIT" tag
        screen_.showPreinfusion(extraction_.volumeMl(),
                                extraction_.phaseSecondsLeft(),
                                extraction_.state() == EXTRACTION_PREINFUSION_WAIT);
        break;
      default:
        // Main extraction, or final values of an extraction that just stopped
        screen_.showExtractionProgress(extraction_.volumeMl(), extraction_.durationSeconds());
        break;
    }
  }
  wasExtracting_ = extracting;

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
  // The target volume of a running extraction cannot be changed
  if (screen_.activeParameter() == PARAMETER_VOLUME && extraction_.isActive()) {
    return;
  }
  screen_.adjustActiveParameter(delta);
}

void CoffeeMachine::handleShortPush() {
  if (extraction_.isActive()) {
    extraction_.stop(EXTRACTION_FINISHED);
  } else if (screen_.activeParameter() == PARAMETER_VOLUME) {
    extraction_.start(encoder_.eventPressTime());
    screen_.showExtractionStarted();
  }
}

void CoffeeMachine::handleDoublePush() {
  if (extraction_.isActive()) {
    extraction_.stop(EXTRACTION_FINISHED);
  }
  screen_.selectNextParameter();
}

void CoffeeMachine::handleLongPush() {
  // A long push is ignored during an extraction (any phase, pre-infusion included):
  // nothing is saved and the saved-value blink is not shown.
  if (extraction_.isActive()) {
    return;
  }

  const Settings &settings = screen_.settings();

  if (screen_.activeParameter() == PARAMETER_VOLUME) {
    store_.saveVolume(settings.volume);
  } else {
    store_.saveTemperature(settings.temperature);
    heater_.setSetpoint(settings.temperature);
  }

  screen_.confirmSave(millis());
}
