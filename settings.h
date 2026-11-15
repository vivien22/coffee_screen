/*
  settings: the values the user can change and save (temperature setpoint, volume).
  Stored in EEPROM with a magic byte: a blank or old-layout EEPROM gives the defaults,
  and the loaded values are always clamped to their valid range.
*/

#pragma once
#include <Arduino.h>

struct Settings {
  int16_t temperature;   // degrees C
  int16_t volume;        // ml
};

class SettingsStore {
  public:
    Settings load() const;
    void saveTemperature(int16_t value);
    void saveVolume(int16_t value);
};
