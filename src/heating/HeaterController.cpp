// HeaterController.cpp: PID window, feedforward from the water flow, full power when far below the setpoint.

#include "HeaterController.h"
#include "../Config.h"
#include "../Debug.h"

// The PID works on the members input_, output_ and setpoint_ (PID_v1 keeps pointers to them).
HeaterController::HeaterController(uint8_t relayPin) :
  relayPin_(relayPin),
  setpoint_(0.0),
  input_(0.0),
  output_(0.0),
  pid_(&input_, &output_, &setpoint_, PID_KP, PID_KI, PID_KD, P_ON_E, DIRECT),
  on_(false),
  manualMode_(false),
  feedforwardMs_(0),
  windowStart_(0),
  lastDebugAt_(0)
{
}

// The relay is off; the PID output goes from 0 to the length of one window.
void HeaterController::begin() {
  pinMode(relayPin_, OUTPUT);
  digitalWrite(relayPin_, LOW);
  on_ = false;

  pid_.SetOutputLimits(0, PID_WINDOW_MS);
  pid_.SetSampleTime(SENSOR_PERIOD_MS);
  pid_.SetMode(AUTOMATIC);

  windowStart_ = millis();
}

// Setpoint of the PROBE (the machine adds TEMPERATURE_OFFSET to the water temperature wanted).
void HeaterController::setSetpoint(double celsius) {
  setpoint_ = celsius;
}

// One pass. The relay is switched on for part of each PID_WINDOW_MS window (time-proportional control):
//   - invalid temperature (or standby): relay off
//   - far below the setpoint: full power, the PID stays off so that it cannot wind up
//   - otherwise: on time = feedforward (from the water flow) + PID correction, kept inside the window
void HeaterController::update(uint32_t now, bool temperatureValid, double temperature, float inflowMlPerSec) {
  if (!temperatureValid) {
    feedforwardMs_ = 0;
    setRelay(false);
    return;
  }

  input_ = temperature;

  if (setpoint_ - input_ > PID_WORKING_RANGE) {
    // Far below the setpoint: full power, the PID would only wind up.
    if (!manualMode_) {
      pid_.SetMode(MANUAL);
      output_ = PID_WINDOW_MS;
      manualMode_ = true;
      DEBUG_PRINTLN(F("PID off, manual mode"));
    }
    feedforwardMs_ = 0;
  } else {
    if (manualMode_) {
      output_ = 0;
      manualMode_ = false;
    }

    // The PID works around the feedforward: it can add to it or take from it, but the total
    // always stays between 0 and the window (so the PID integral cannot wind up).
    feedforwardMs_ = computeFeedforwardMs(inflowMlPerSec);
    pid_.SetMode(AUTOMATIC);
    pid_.SetOutputLimits(-feedforwardMs_, PID_WINDOW_MS - feedforwardMs_);
    pid_.Compute();
  }

  if ((uint32_t)(now - windowStart_) >= (uint32_t)PID_WINDOW_MS) {
    windowStart_ = now;
  }

  const uint32_t elapsedInWindow = (uint32_t)(now - windowStart_);
  const int onTimeMs = constrain((int)output_ + feedforwardMs_, 0, PID_WINDOW_MS);
  setRelay(elapsedInWindow < (uint32_t)onTimeMs);

  if ((uint32_t)(now - lastDebugAt_) >= 1000UL) {
    lastDebugAt_ = now;
    DEBUG_PRINT(F("Output = ")); DEBUG_PRINT(onTimeMs);
    DEBUG_PRINT(F(" FeedforwardMs = ")); DEBUG_PRINT(feedforwardMs_);
    DEBUG_PRINT(F(" Temperature = ")); DEBUG_PRINTLN(input_);
  }
}

// Heater on-time (ms per PID window) needed to heat the water that leaves the boiler.
int HeaterController::computeFeedforwardMs(float inflowMlPerSec) const {
  const float heating = (float)setpoint_ - WATER_INLET_TEMPERATURE;   // degrees to give to the new water

  if (inflowMlPerSec <= 0.0f || heating <= 0.0f) {
    return 0;
  }

  const float powerW = inflowMlPerSec * WATER_SPECIFIC_HEAT * heating * FEEDFORWARD_GAIN;
  const float ms = powerW / HEATER_POWER_W * (float)PID_WINDOW_MS;

  return constrain((int)(ms + 0.5f), 0, PID_WINDOW_MS);
}

// Switches the relay only when its state changes.
void HeaterController::setRelay(bool on) {
  if (on == on_) {
    return;
  }
  on_ = on;
  digitalWrite(relayPin_, on ? HIGH : LOW);
}
