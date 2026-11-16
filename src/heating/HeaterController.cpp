// HeaterController.cpp: PID window, feedforward from the (delayed) water flow, full power when far below the setpoint.

#include "HeaterController.h"
#include "../Config.h"

// The flow history keeps the flow in steps of 0.05 ml/s in one byte: up to 12.75 ml/s
static const float FLOW_HISTORY_UNIT = 0.05f;

// The PID works on the members input_, output_ and setpoint_ (PID_v1 keeps pointers to them).
HeaterController::HeaterController(uint8_t relayPin) :
  relayPin_(relayPin),
  setpoint_(DEFAULT_SETPOINT),
  input_(0.0),
  output_(0.0),
  pid_(&input_, &output_, &setpoint_, PID_KP, PID_KI, PID_KD, P_ON_E, DIRECT),
  kp_(PID_KP),
  ki_(PID_KI),
  kd_(PID_KD),
  feedforwardGain_(FEEDFORWARD_GAIN),
  feedforwardDelaySteps_((uint8_t)(FEEDFORWARD_DELAY_MS / FEEDFORWARD_STEP_MS)),
  heaterPowerW_(HEATER_POWER_W),
  inletTemperature_(WATER_INLET_TEMPERATURE),
  enabled_(true),
  on_(false),
  manualMode_(false),
  feedforwardMs_(0),
  onTimeMs_(0),
  windowStart_(0),
  flowHead_(0),
  lastFlowStoredAt_(0)
{
  for (uint8_t i = 0; i < FEEDFORWARD_HISTORY; i++) {
    flowHistory_[i] = 0;
  }
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
  lastFlowStoredAt_ = windowStart_;
}

void HeaterController::setSetpoint(double celsius) {
  setpoint_ = constrain(celsius, 0.0, (double)PROBE_SETPOINT_MAX);
}

void HeaterController::setTunings(double kp, double ki, double kd) {
  kp_ = kp;
  ki_ = ki;
  kd_ = kd;
  pid_.SetTunings(kp, ki, kd);
}

void HeaterController::setFeedforwardDelayMs(uint32_t delayMs) {
  const uint32_t steps = delayMs / FEEDFORWARD_STEP_MS;
  feedforwardDelaySteps_ = (steps > FEEDFORWARD_HISTORY - 1) ? (FEEDFORWARD_HISTORY - 1) : (uint8_t)steps;
}

// One pass. The relay is switched on for part of each PID_WINDOW_MS window (time-proportional control):
//   - invalid temperature, heater disabled or no setpoint: relay off
//   - far below the setpoint: full power, the PID stays off so that it cannot wind up
//   - otherwise: on time = feedforward (from the delayed water flow) + PID correction, kept inside the window
void HeaterController::update(uint32_t now, bool temperatureValid, double temperature, float inflowMlPerSec) {
  storeFlow(now, inflowMlPerSec);

  if (!temperatureValid || !enabled_ || setpoint_ <= 0.0) {
    feedforwardMs_ = 0;
    onTimeMs_ = 0;
    output_ = 0;
    if (!manualMode_) {
      pid_.SetMode(MANUAL);   // the integral term starts again from 0 when the heater is back
      manualMode_ = true;
    }
    setRelay(false);
    return;
  }

  input_ = temperature;

  if (setpoint_ - input_ > PID_WORKING_RANGE) {
    // Far below the setpoint: full power, the PID would only wind up.
    if (!manualMode_) {
      pid_.SetMode(MANUAL);
      manualMode_ = true;
    }
    output_ = PID_WINDOW_MS;
    feedforwardMs_ = 0;
  } else {
    if (manualMode_) {
      output_ = 0;
      manualMode_ = false;
    }

    // The PID works around the feedforward: it can add to it or take from it, but the total
    // always stays between 0 and the window (so the PID integral cannot wind up).
    feedforwardMs_ = computeFeedforwardMs(delayedFlow());
    pid_.SetMode(AUTOMATIC);
    pid_.SetOutputLimits(-feedforwardMs_, PID_WINDOW_MS - feedforwardMs_);
    pid_.Compute();
  }

  if ((uint32_t)(now - windowStart_) >= (uint32_t)PID_WINDOW_MS) {
    windowStart_ = now;
  }

  const uint32_t elapsedInWindow = (uint32_t)(now - windowStart_);
  onTimeMs_ = constrain((int)output_ + feedforwardMs_, 0, PID_WINDOW_MS);
  setRelay(elapsedInWindow < (uint32_t)onTimeMs_);
}

// Remembers the flow every FEEDFORWARD_STEP_MS (the oldest value is overwritten)
void HeaterController::storeFlow(uint32_t now, float inflowMlPerSec) {
  if ((uint32_t)(now - lastFlowStoredAt_) < FEEDFORWARD_STEP_MS) {
    return;
  }
  lastFlowStoredAt_ += FEEDFORWARD_STEP_MS;
  if ((uint32_t)(now - lastFlowStoredAt_) >= FEEDFORWARD_STEP_MS) {
    lastFlowStoredAt_ = now;   // the loop was late: do not catch up step by step
  }

  float units = inflowMlPerSec / FLOW_HISTORY_UNIT + 0.5f;
  units = constrain(units, 0.0f, 255.0f);
  flowHead_ = (flowHead_ + 1) % FEEDFORWARD_HISTORY;
  flowHistory_[flowHead_] = (uint8_t)units;
}

// The flow (ml/s) that was measured feedforwardDelaySteps_ steps ago (0 = the last one stored)
float HeaterController::delayedFlow() const {
  const uint8_t index = (flowHead_ + FEEDFORWARD_HISTORY - feedforwardDelaySteps_) % FEEDFORWARD_HISTORY;
  return flowHistory_[index] * FLOW_HISTORY_UNIT;
}

// Heater on-time (ms per PID window) needed to heat the water that enters the boiler.
int HeaterController::computeFeedforwardMs(float inflowMlPerSec) const {
  const float heating = (float)setpoint_ - inletTemperature_;   // degrees to give to the new water

  if (inflowMlPerSec <= 0.0f || heating <= 0.0f || heaterPowerW_ <= 0.0f) {
    return 0;
  }

  const float powerW = inflowMlPerSec * WATER_SPECIFIC_HEAT * heating * feedforwardGain_;
  const float ms = powerW / heaterPowerW_ * (float)PID_WINDOW_MS;

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
