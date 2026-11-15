/*
  settings: the values the user can change and save (temperature setpoint, volume,
  pre-infusion time, wait time and flush time).
  Stored in EEPROM with a magic byte: a blank or unknown EEPROM gives the defaults, the previous
  layouts (without the times, or without the flush time) are migrated, and the loaded values are always clamped to
  their valid range.
*/

#pragma once
#include <Arduino.h>

struct Settings {
  int16_t temperature;   // degrees C
  int16_t volume;        // ml
  int16_t preinfusion;       // s, 0 = no pre-infusion
  int16_t preinfusionWait;   // s, 0 = no wait
  int16_t flush;             // s, 0 = no flush
};

class SettingsStore {
  public:
    Settings load() const;
    void saveTemperature(int16_t value);
    void saveVolume(int16_t value);
    void savePreinfusion(int16_t seconds);
    void savePreinfusionWait(int16_t seconds);
    void saveFlush(int16_t seconds);
};
