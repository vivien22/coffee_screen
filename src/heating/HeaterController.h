/*
  HeaterController: time-proportional heater control.
  PID + feedforward on the flow of water entering the boiler, over a PID_WINDOW_MS window.
  Takes its inputs as parameters (temperature, flow): it does not know the sensor,
  the extraction or the screen.
*/

#pragma once
#include <Arduino.h>
#include <PID_v1.h>

class HeaterController {
  public:
    explicit HeaterController(uint8_t relayPin);

    // The relay is switched off, the PID is configured.
    void begin();

    void setSetpoint(double celsius);

    // Call on every loop pass.
    // If the temperature is not valid, the heater is switched off.
    // inflowMlPerSec: flow of the water entering the boiler (0 when nothing flows).
    void update(uint32_t now, bool temperatureValid, double temperature, float inflowMlPerSec);

    bool isOn() const { return on_; }

    // The relay stays off for the next ms milliseconds (the PID goes on computing). Used when the pump starts: the relays of the
    // pump and the valve have to pull in while the heater relay is not drawing (see HEATER_PAUSE_AT_PUMP_START_MS in Config.h).
    void pause(uint32_t now, uint32_t ms);

  private:
    int computeFeedforwardMs(float inflowMlPerSec) const;
    void setRelay(bool on);

    uint8_t relayPin_;

    // PID_v1 works on pointers to these three values
    double setpoint_;
    double input_;
    double output_;
    PID pid_;

    bool on_;
    bool paused_ = false;       // the relay is held off until pausedUntil_
    uint32_t pausedUntil_ = 0;
    bool manualMode_;           // far from the setpoint: full power, PID off
    int feedforwardMs_;         // part of the window given by the feedforward; the PID output is added to it
    uint32_t windowStart_;
    uint32_t lastDebugAt_;
};
