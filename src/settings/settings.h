/*
  settings: the values the user can change and save (temperature setpoint, volume,
  pre-infusion volume, wait time, flush time, mode and MANUAL time).
  Stored in EEPROM with a magic byte: a blank or unknown EEPROM gives the defaults, and the
  loaded values are always clamped to their valid range. No migration of older layouts.
*/

#pragma once
#include <Arduino.h>

struct Settings {
  int16_t temperature;   // degrees C
  int16_t volume;        // ml
  int16_t preinfusion;       // ml, 0 = no pre-infusion
  int16_t preinfusionWait;   // s, 0 = no wait
  int16_t flush;             // s, 0 = no flush
  int16_t manualSeconds;     // s, MANUAL mode: 0 = the pump runs until the second push
  bool manual;               // MANUAL mode (the pump runs between two pushes), false = AUTO
};

class SettingsStore {
  public:
    Settings load() const;
    void saveTemperature(int16_t value);
    void saveVolume(int16_t value);
    void savePreinfusion(int16_t seconds);
    void savePreinfusionWait(int16_t seconds);
    void saveFlush(int16_t seconds);
    void saveManual(bool manual);
    void saveManualSeconds(int16_t seconds);
};
