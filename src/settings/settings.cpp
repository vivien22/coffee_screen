#include "settings.h"
#include "../config.h"
#include <EEPROM.h>

// EEPROM layout: [magic: 1 byte][temperature: int16][volume: int16][pre-infusion: 1 byte][wait: 1 byte][flush: 1 byte]
// The magic byte only tells a written EEPROM from a blank one. There is no migration of older layouts:
// change the magic value when the layout changes, the settings then take their defaults once.
static const uint8_t SETTINGS_MAGIC    = 0xA7;
static const int ADDR_MAGIC            = 0;
static const int ADDR_TEMPERATURE      = ADDR_MAGIC + 1;
static const int ADDR_VOLUME           = ADDR_TEMPERATURE + (int)sizeof(int16_t);
static const int ADDR_PREINFUSION      = ADDR_VOLUME + (int)sizeof(int16_t);
static const int ADDR_PREINFUSION_WAIT = ADDR_PREINFUSION + 1;
static const int ADDR_FLUSH            = ADDR_PREINFUSION_WAIT + 1;

Settings SettingsStore::load() const {
  Settings settings;

  if (EEPROM.read(ADDR_MAGIC) == SETTINGS_MAGIC) {
    // Saved values, clamped to their valid range
    EEPROM.get(ADDR_TEMPERATURE, settings.temperature);
    EEPROM.get(ADDR_VOLUME, settings.volume);
    settings.temperature = constrain(settings.temperature, VALUE_MIN, TEMPERATURE_MAX);
    settings.volume = constrain(settings.volume, VALUE_MIN, VOLUME_MAX);
    settings.preinfusion = constrain(EEPROM.read(ADDR_PREINFUSION), VALUE_MIN, PREINFUSION_MAX_SECONDS);
    settings.preinfusionWait = constrain(EEPROM.read(ADDR_PREINFUSION_WAIT), VALUE_MIN, PREINFUSION_MAX_SECONDS);
    settings.flush = constrain(EEPROM.read(ADDR_FLUSH), VALUE_MIN, FLUSH_MAX_SECONDS);
  } else {
    // Blank or unknown EEPROM: defaults, written at once
    settings.temperature = DEFAULT_TEMPERATURE;
    settings.volume = DEFAULT_VOLUME;
    settings.preinfusion = DEFAULT_PREINFUSION_SECONDS;
    settings.preinfusionWait = DEFAULT_PREINFUSION_WAIT_SECONDS;
    settings.flush = DEFAULT_FLUSH_SECONDS;
    EEPROM.put(ADDR_TEMPERATURE, settings.temperature);
    EEPROM.put(ADDR_VOLUME, settings.volume);
    EEPROM.update(ADDR_PREINFUSION, (uint8_t)settings.preinfusion);
    EEPROM.update(ADDR_PREINFUSION_WAIT, (uint8_t)settings.preinfusionWait);
    EEPROM.update(ADDR_FLUSH, (uint8_t)settings.flush);
    EEPROM.update(ADDR_MAGIC, SETTINGS_MAGIC);
  }

  return settings;
}

// EEPROM.put / update only write the bytes that changed.
void SettingsStore::saveTemperature(int16_t value) {
  EEPROM.put(ADDR_TEMPERATURE, value);
}

void SettingsStore::saveVolume(int16_t value) {
  EEPROM.put(ADDR_VOLUME, value);
}

void SettingsStore::savePreinfusion(int16_t seconds) {
  EEPROM.update(ADDR_PREINFUSION, (uint8_t)seconds);
}

void SettingsStore::savePreinfusionWait(int16_t seconds) {
  EEPROM.update(ADDR_PREINFUSION_WAIT, (uint8_t)seconds);
}

void SettingsStore::saveFlush(int16_t seconds) {
  EEPROM.update(ADDR_FLUSH, (uint8_t)seconds);
}
