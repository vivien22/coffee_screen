#include "settings.h"
#include "config.h"
#include <EEPROM.h>

// EEPROM layout: [magic: 1 byte][temperature: int16][volume: int16][pre-infusion: 1 byte][wait: 1 byte][flush: 1 byte]
// Previous layouts are migrated: the values they hold are kept and the missing ones take their defaults.
//   magic 0xA5: temperature, volume
//   magic 0xA6: temperature, volume, pre-infusion, wait
//   magic 0xA7: all of them (current)
static const uint8_t SETTINGS_MAGIC    = 0xA7;
static const uint8_t SETTINGS_MAGIC_V2 = 0xA6;
static const uint8_t SETTINGS_MAGIC_V1 = 0xA5;
static const int ADDR_MAGIC            = 0;
static const int ADDR_TEMPERATURE      = ADDR_MAGIC + 1;
static const int ADDR_VOLUME           = ADDR_TEMPERATURE + (int)sizeof(int16_t);
static const int ADDR_PREINFUSION      = ADDR_VOLUME + (int)sizeof(int16_t);
static const int ADDR_PREINFUSION_WAIT = ADDR_PREINFUSION + 1;
static const int ADDR_FLUSH            = ADDR_PREINFUSION_WAIT + 1;

Settings SettingsStore::load() const {
  Settings settings;
  const uint8_t magic = EEPROM.read(ADDR_MAGIC);
  const bool hasTimes = (magic == SETTINGS_MAGIC || magic == SETTINGS_MAGIC_V2);   // pre-infusion and wait
  const bool hasFlush = (magic == SETTINGS_MAGIC);

  // Temperature and volume: present in every layout
  if (hasTimes || magic == SETTINGS_MAGIC_V1) {
    EEPROM.get(ADDR_TEMPERATURE, settings.temperature);
    EEPROM.get(ADDR_VOLUME, settings.volume);
    settings.temperature = constrain(settings.temperature, VALUE_MIN, TEMPERATURE_MAX);
    settings.volume = constrain(settings.volume, VALUE_MIN, VOLUME_MAX);
  } else {
    settings.temperature = DEFAULT_TEMPERATURE;
    settings.volume = DEFAULT_VOLUME;
    EEPROM.put(ADDR_TEMPERATURE, settings.temperature);
    EEPROM.put(ADDR_VOLUME, settings.volume);
  }

  // Pre-infusion and wait times
  if (hasTimes) {
    const uint8_t preinfusion = EEPROM.read(ADDR_PREINFUSION);
    const uint8_t preinfusionWait = EEPROM.read(ADDR_PREINFUSION_WAIT);
    settings.preinfusion = constrain(preinfusion, VALUE_MIN, PREINFUSION_MAX_SECONDS);
    settings.preinfusionWait = constrain(preinfusionWait, VALUE_MIN, PREINFUSION_MAX_SECONDS);
  } else {
    settings.preinfusion = DEFAULT_PREINFUSION_SECONDS;
    settings.preinfusionWait = DEFAULT_PREINFUSION_WAIT_SECONDS;
    EEPROM.update(ADDR_PREINFUSION, (uint8_t)settings.preinfusion);
    EEPROM.update(ADDR_PREINFUSION_WAIT, (uint8_t)settings.preinfusionWait);
  }

  // Flush time
  if (hasFlush) {
    const uint8_t flush = EEPROM.read(ADDR_FLUSH);
    settings.flush = constrain(flush, VALUE_MIN, FLUSH_MAX_SECONDS);
  } else {
    settings.flush = DEFAULT_FLUSH_SECONDS;
    EEPROM.update(ADDR_FLUSH, (uint8_t)settings.flush);
  }

  // Everything is now in the current layout
  EEPROM.update(ADDR_MAGIC, SETTINGS_MAGIC);

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
