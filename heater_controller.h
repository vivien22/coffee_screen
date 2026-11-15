/*
  heater_controller: time-proportional heater control.
  PID + feedforward on the flow of water leaving the boiler, over a PID_WINDOW_MS window.
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
    // outflowMlPerSec: flow of the water leaving the boiler (0 when nothing flows).
    void update(uint32_t now, bool temperatureValid, double temperature, float outflowMlPerSec);

    bool isOn() const { return on_; }

  private:
    int computeFeedforwardMs(float outflowMlPerSec) const;
    void setRelay(bool on);

    uint8_t relayPin_;

    // PID_v1 works on pointers to these three values
    double setpoint_;
    double input_;
    double output_;
    PID pid_;

    bool on_;
    bool manualMode_;           // far from the setpoint: full power, PID off
    int feedforwardMs_;         // part of the window given by the feedforward; the PID output is added to it
    uint32_t windowStart_;
    uint32_t lastDebugAt_;
};
