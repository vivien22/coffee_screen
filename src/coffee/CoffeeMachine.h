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
#include "../settings/Journal.h"
#include "FlowRecorder.h"
#include "../drivers/SerialConsole.h"
#include "../drivers/Perf.h"
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
    void handlePush(PushEvent pushEvent);   // the push events of the encoder: short and double (the long push is disabled)
    void handleShortPush();    // the short push of the encoder: switch between AUTO and MANUAL, or clear the no-water error (ignored during an extraction)
    void handleExtractionPush();   // what the start / stop button does while an extraction runs: end it / start the flush / stop the flush
    void handleDoublePush();   // select the next parameter (volume, pre-infusion, wait, flush, temperature); ignored during
                               // the extraction, skips the flush if it is waiting or running
    void toggleSteaming();     // the long push: enter / leave the steaming mode (nothing going on)
    void writeJournal();       // one line in the journal of the last extractions
    void validateCalibration();   // CAL validated: the weight goes to the journal (if changed), the loss is corrected
    void updateExtraction(uint32_t now);   // sections 2 and 4 of update()
    void showExtraction(uint32_t now);
    void saveSettings();       // save the settings (changed bytes only) and apply the temperature setpoint
    void wakeUp();             // end of the standby: the heater works again, the real screen comes back
    void adjustActiveParameter(int16_t delta);

    FlowSensor flow_;
    CJMCU111 encoder_;
    SettingsStore store_;
    Journal journal_;
    FlowRecorder recorder_;   // the curve of the flow In and Out of the extraction, written in the journal with its entry
    SerialConsole console_;   // the commands of the USB serial line (j = the journal, p = the performance trace)
    TemperatureSensor temperature_;
    HeaterController heater_;
    ExtractionController extraction_;
    ScreenController screen_;

    bool wasExtracting_;   // to show the final values of an extraction
    ExtractionState lastState_ = EXTRACTION_IDLE;   // state seen while the extraction was active (to know if a flush ended)
    bool steaming_ = false;           // steaming mode (see toggleSteaming)
    uint8_t journalTempStart_ = 0;    // the temperature shown when the extraction started (for the journal)
    bool journalPending_ = false;     // an extraction was started and is not in the journal yet
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
