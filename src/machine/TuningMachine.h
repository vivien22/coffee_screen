/*
  TuningMachine: the firmware of the pid-tuning branch. It connects the temperature sensor, the heater
  controller, the flow sensors and the pump, and gives them a serial console:

      serial commands ──> heater tuning, setpoint, pump
      temperature sensor, flow sensors ──> heater controller
      everything ──> one CSV line per TELEMETRY_PERIOD_MS on the serial port

  There is no screen and no user interface other than the serial port. The commands are listed by "help"
  (see also the README).
*/

#pragma once
#include <Arduino.h>

#include "../drivers/FlowSensor.h"
#include "../heating/TemperatureSensor.h"
#include "../heating/HeaterController.h"

class TuningMachine {
  public:
    TuningMachine();

    // Call in setup(), after Serial.begin()
    void begin();

    // Call on every loop pass: cooperative loop, no delay().
    void update();

    // Runs one command line ("kp 35", "pump 20"...). The line is modified. Prints the answer on the serial port.
    // Public so that the tests can send commands without a serial port.
    void execute(char *line);

    const HeaterController &heater() const { return heater_; }
    bool pumpIsOn() const { return pumpOn_; }
    uint32_t telemetryPeriodMs() const { return telemetryPeriodMs_; }

  private:
    void readSerial();
    void startPump(uint32_t now, uint16_t seconds);
    void stopPump(const __FlashStringHelper *reason);
    void updatePump(uint32_t now);
    void printHeader();
    void printTelemetry(uint32_t now);
    void printParameters();
    void printHelp();

    FlowSensor flow_;
    TemperatureSensor temperature_;
    HeaterController heater_;

    bool pumpOn_;
    uint32_t pumpEndAt_;
    uint32_t noFlowCheckAt_;
    float noFlowCheckMl_;

    uint32_t telemetryPeriodMs_;
    uint32_t lastTelemetryAt_;

    char line_[24];       // the command being typed
    uint8_t lineLength_;
};
