/*
  coffee_machine: the application. It owns every module and connects them:

      encoder ──> user actions ──> screen / extraction / settings / heater setpoint
      temperature sensor ───────> heater controller (+ flow from the extraction)
      extraction, temperature, heater ──> screen

  The modules (TemperatureSensor, HeaterController, ExtractionController, ScreenController,
  SettingsStore) do not know each other: all the decisions that involve several of them
  are here.
*/

#pragma once
#include <Arduino.h>

#include "CJMCU111.h"
#include "USN_FlowSensor.h"
#include "settings.h"
#include "temperature_sensor.h"
#include "heater_controller.h"
#include "extraction_controller.h"
#include "screen_controller.h"

class CoffeeMachine {
  public:
    CoffeeMachine();

    // Call in setup()
    void begin();

    // Call on every loop pass: cooperative loop, no delay().
    void update();

  private:
    // User actions
    void pollEncoder();
    void handleShortPush();    // start / stop the extraction
    void handleDoublePush();   // stop the extraction, select the next parameter
    void handleLongPush();     // save the selected parameter (ignored during an extraction)
    void adjustActiveParameter(int16_t delta);

    USN_FlowSensor flow_;
    CJMCU111 encoder_;
    SettingsStore store_;
    TemperatureSensor temperature_;
    HeaterController heater_;
    ExtractionController extraction_;
    ScreenController screen_;

    bool wasExtracting_;   // to show the final values of an extraction
};
