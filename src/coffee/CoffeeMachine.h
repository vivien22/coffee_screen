/*
  CoffeeMachine: the application. It owns every module and connects them:

      encoder ──> user actions ──> screen / extraction / settings / heater setpoint
      temperature sensor ───────> heater controller (+ flow from the extraction)
      extraction, temperature, heater ──> screen

  The modules (TemperatureSensor, HeaterController, ExtractionController, ScreenController,
  SettingsStore) do not know each other: all the decisions that involve several of them
  are here.
*/

#pragma once
#include <Arduino.h>

#include "../drivers/CJMCU111.h"
#include "../drivers/FlowSensor.h"
#include "../settings/Settings.h"
#include "../heating/TemperatureSensor.h"
#include "../heating/HeaterController.h"
#include "ExtractionController.h"
#include "../display/ScreenController.h"

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
    void pollStartButton();    // the start / stop button (START_BUTTON): debounced, acts on the push
    void handleStartButton();  // extraction running: the push of the encoder; nothing running: keep the settings, volume, start
    void startExtraction(uint32_t pressTime);   // keep the settings and start the extraction (AUTO or MANUAL)
    void handleShortPush();    // start the extraction / end it / start the flush / stop the flush
    void handleDoublePush();   // select the next parameter (volume, pre-infusion, wait, flush, temperature); ignored during
                               // the extraction, skips the flush if it is waiting or running
    void handleLongPush();     // switch between AUTO and MANUAL, or clear the no-water error (ignored during an extraction)
    void saveSettings();       // save the settings (changed bytes only) and apply the temperature setpoint
    void wakeUp();             // end of the standby: the heater works again, the real screen comes back
    void adjustActiveParameter(int16_t delta);

    FlowSensor flow_;
    CJMCU111 encoder_;
    SettingsStore store_;
    TemperatureSensor temperature_;
    HeaterController heater_;
    ExtractionController extraction_;
    ScreenController screen_;

    bool wasExtracting_;   // to show the final values of an extraction
    ExtractionState lastState_ = EXTRACTION_IDLE;   // state seen while the extraction was active (to know if a flush ended)
    bool standby_ = false;            // the heater is off after STANDBY_MINUTES without activity
    uint32_t lastActivityAt_ = 0;
    bool calibrationOffered_ = false; // CAL was offered for the last extraction
    bool resultShown_ = false;        // the values of an extraction without flush are still on the screen
#ifdef START_BUTTON
    bool buttonRaw_ = false;          // the level of the start / stop button now (true = pushed) ...
    bool buttonDown_ = false;         // ... and the debounced one
    uint32_t buttonChangedAt_ = 0;
#endif
    uint32_t resultShownUntil_ = 0;   // end of that display (EXTRACTION_RESULT_DISPLAY_MS after the end)
};
