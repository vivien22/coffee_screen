/*
  settings: the values the user can change and save (temperature setpoint, volume,
  steaming temperature, pre-infusion volume, wait time, flush time, mode and MANUAL time).
  Stored in EEPROM with a magic byte: a blank or unknown EEPROM gives the defaults, and the
  loaded values are always clamped to their valid range. No migration of older layouts.
*/

#pragma once
#include <Arduino.h>

struct Settings {
  int16_t temperature;   // degrees C (BrewingTemp: the setpoint of the brewing)
  int16_t volume;        // ml
  int16_t preinfusion;       // ml, 0 = no pre-infusion
  int16_t preinfusionWait;   // s, 0 = no wait
  int16_t flush;             // s, 0 = no flush
  int16_t manualSeconds;     // s, MANUAL mode: 0 = the pump runs until the second push
  float loss;                // ml, water that does not end in the cup (puck, backflush), see Config.h
  bool manual;               // MANUAL mode (the pump runs between two pushes), false = AUTO
  int16_t steamingTemperature;   // degrees C (SteamingTemp: the setpoint of the steaming mode)
  int16_t flowLimit;             // tenths of ml/s, the flow limit of the main extraction, 0 = off (see coffee/FlowLimit.h)
};

class SettingsStore {
  public:
    Settings load() const;

    // Writes the settings (the 22 bytes at address 0) in ONE block operation of the data flash, and none when nothing changed
    // (settings/Flash.h): it can be called as often as wanted without wearing the EEPROM. The machine calls it a moment after the
    // last change, never during an extraction (CoffeeMachine).
    void save(const Settings &settings) const;

    // The delay (seconds of flow) by which the pump stops before the target, learned by the machine (STOP_DELAY_* in
    // Config.h). Not a setting of the user: it has its own place, after the loss. A blank EEPROM gives the default.
    float loadStopDelay() const;
    void saveStopDelay(float seconds) const;
};
