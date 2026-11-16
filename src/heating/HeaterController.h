/*
  HeaterController: time-proportional heater control.
  PID + feedforward on the flow of water entering the boiler, over a PID_WINDOW_MS window.
  The tuning values (gains, feedforward gain and delay, heater power, inlet temperature) can be changed while
  it runs. It takes its inputs as parameters (temperature, flow): it does not know the sensor or the pump.
*/

#pragma once
#include <Arduino.h>
#include <PID_v1.h>
#include "../Config.h"

class HeaterController {
  public:
    explicit HeaterController(uint8_t relayPin);

    // The relay is switched off, the PID is configured.
    void begin();

    // ----- Tuning (all can be changed at any time) -----

    // Setpoint of the probe, kept between 0 and PROBE_SETPOINT_MAX. 0 or less: heater off.
    void setSetpoint(double celsius);
    void setTunings(double kp, double ki, double kd);
    void setFeedforwardGain(float gain) { feedforwardGain_ = gain; }
    // The feedforward uses the flow measured delayMs ago (rounded down to FEEDFORWARD_STEP_MS, at most 19.75 s)
    void setFeedforwardDelayMs(uint32_t delayMs);
    void setHeaterPowerW(float watts) { heaterPowerW_ = watts; }
    void setInletTemperature(float celsius) { inletTemperature_ = celsius; }
    // false: the heater stays off whatever the controller computes
    void setEnabled(bool enabled) { enabled_ = enabled; }

    double setpoint() const { return setpoint_; }
    double kp() const { return kp_; }
    double ki() const { return ki_; }
    double kd() const { return kd_; }
    float feedforwardGain() const { return feedforwardGain_; }
    uint32_t feedforwardDelayMs() const { return feedforwardDelaySteps_ * FEEDFORWARD_STEP_MS; }
    float heaterPowerW() const { return heaterPowerW_; }
    float inletTemperature() const { return inletTemperature_; }
    bool enabled() const { return enabled_; }

    // ----- Running -----

    // Call on every loop pass.
    // If the temperature is not valid, the heater is switched off.
    // inflowMlPerSec: flow of the water entering the boiler (0 when nothing flows).
    void update(uint32_t now, bool temperatureValid, double temperature, float inflowMlPerSec);

    bool isOn() const { return on_; }

    // What was computed for the current window (ms of heating per window), for the telemetry
    int pidMs() const { return (int)output_; }
    int feedforwardMs() const { return feedforwardMs_; }
    int onTimeMs() const { return onTimeMs_; }

  private:
    void storeFlow(uint32_t now, float inflowMlPerSec);
    float delayedFlow() const;
    int computeFeedforwardMs(float inflowMlPerSec) const;
    void setRelay(bool on);

    uint8_t relayPin_;

    // PID_v1 works on pointers to these three values
    double setpoint_;
    double input_;
    double output_;
    PID pid_;
    double kp_, ki_, kd_;

    float feedforwardGain_;
    uint8_t feedforwardDelaySteps_;
    float heaterPowerW_;
    float inletTemperature_;
    bool enabled_;

    bool on_;
    bool manualMode_;           // far from the setpoint: full power, PID off
    int feedforwardMs_;         // part of the window given by the feedforward; the PID output is added to it
    int onTimeMs_;
    uint32_t windowStart_;

    // Flow of the last FEEDFORWARD_HISTORY steps, in units of FLOW_HISTORY_UNIT ml/s (a byte each)
    uint8_t flowHistory_[FEEDFORWARD_HISTORY];
    uint8_t flowHead_;
    uint32_t lastFlowStoredAt_;
};
