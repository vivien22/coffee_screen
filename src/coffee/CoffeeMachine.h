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
#include "../drivers/StartButton.h"
#include "../drivers/FlowSensor.h"
#include "../settings/Settings.h"
#include "ExtractionJournal.h"
#include "BoilerRefill.h"
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
    void pollStartButton();    // the start / stop button (START_BUTTON): acts on the push, and on the long push
    void switchToManual(uint32_t now);   // the long push of the start button: the AUTO extraction becomes a MANUAL one
    void handleStartButton();  // extraction running: the push of the encoder; nothing running: keep the settings, volume, start
    void startExtraction(uint32_t pressTime);   // keep the settings and start the extraction (AUTO or MANUAL)
    void handlePush(PushEvent pushEvent);   // the push events of the encoder: short and double (the long push is disabled)
    void handleShortPush();    // the short push of the encoder: switch between AUTO and MANUAL, or clear the no-water error (ignored during an extraction)
    void handleExtractionPush();   // what the start / stop button does while an extraction runs: end it / start the flush / stop the flush
    void handleDoublePush();   // select the next parameter (volume, pre-infusion, wait, flush, temperature); ignored during
                               // the extraction, skips the flush if it is waiting or running
    void toggleSteaming();     // the long push: enter / leave the steaming mode (nothing going on)
    void leaveSteaming(bool refill);   // refill: the boiler is refilled (the long push; not when the mode times out)
    void updateRefill(uint32_t now);
    void stopRefill();
    void captureJournal();     // the extraction is over: its line for the journal is made (and the recording of its curve stops)
    void validateCalibration();   // CAL validated: the weight goes to the journal (if changed), the loss is corrected
    void updateExtraction(uint32_t now);   // sections 2 and 4 of update()
    void showExtraction(uint32_t now);
    void saveSettings();       // the settings are to be saved (written SETTINGS_SAVE_DELAY_MS after the last change) and the setpoint is applied
    void saveSettingsNow();    // the same, written at once (before an extraction starts)
    void applySetpoint();      // the temperature setpoint of the mode to the heater
    void writeFlash(uint32_t now);   // the writes to the data flash that are due, one block operation at most per pass, never during an extraction
    bool flashQuiet() const;   // nothing that a block operation of the flash could delay is going on
    void wakeUp();             // end of the standby: the heater works again, the real screen comes back
    void adjustActiveParameter(int16_t delta);

    FlowSensor flow_;
    CJMCU111 encoder_;
    SettingsStore store_;
    ExtractionJournal journal_;   // the line and the flow curve of the AUTO extraction, for the journal of the last extractions
    SerialConsole console_;   // the commands of the USB serial line (j = the journal, p = the performance trace)
    TemperatureSensor temperature_;
    HeaterController heater_;
    ExtractionController extraction_;
    ScreenController screen_;
    BoilerRefill refill_;     // the refill of the boiler after the steaming mode

    bool wasExtracting_;   // to show the final values of an extraction
    ExtractionState lastState_ = EXTRACTION_IDLE;   // state seen while the extraction was active (to know if a flush ended)
    bool steaming_ = false;           // steaming mode (see toggleSteaming)
    bool settingsPending_ = false;    // settings changed (pendingSettings_) and not written yet
    Settings pendingSettings_;        // as they were at the last change: what is being edited afterwards is not saved with them
    uint32_t settingsChangedAt_ = 0;
    bool stopDelayPending_ = false;   // the learned stop delay is to be written
    bool standby_ = false;            // the heater is off after STANDBY_MINUTES without activity
    uint32_t lastActivityAt_ = 0;
    bool calibrationOffered_ = false; // CAL was offered for the last extraction
    bool resultShown_ = false;        // the values of an extraction without flush are still on the screen
#ifdef START_BUTTON
    StartButton startButton_{PIN_START_BUTTON};
    bool longPushPending_ = false;    // this push started an AUTO extraction: held LONG_PUSH_MS, it becomes a MANUAL one
    bool autoAfterManual_ = false;    // the running MANUAL extraction came from a long push: AUTO is selected again at its end
#endif
    uint32_t resultShownUntil_ = 0;   // end of that display (EXTRACTION_RESULT_DISPLAY_MS after the end)
};
