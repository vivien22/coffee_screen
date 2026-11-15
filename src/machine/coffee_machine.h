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

#include "../lib/CJMCU111.h"
#include "../lib/USN_FlowSensor.h"
#include "../settings/settings.h"
#include "../heating/temperature_sensor.h"
#include "../heating/heater_controller.h"
#include "extraction_controller.h"
#include "../display/screen_controller.h"

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
    void handleShortPush();    // start the extraction / end it / start the flush / stop the flush
    void handleDoublePush();   // select the next parameter (volume, pre-infusion, wait, flush, temperature); ignored during
                               // the extraction, skips the flush if it is waiting or running
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
    ExtractionState lastState_ = EXTRACTION_IDLE;   // state seen while the extraction was active (to know if a flush ended)
    bool resultShown_ = false;        // the values of an extraction without flush are still on the screen
    uint32_t resultShownUntil_ = 0;   // end of that display (EXTRACTION_RESULT_DISPLAY_MS after the end)
};
