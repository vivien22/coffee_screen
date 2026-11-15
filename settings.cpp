#include "settings.h"
#include "config.h"
#include <EEPROM.h>

// EEPROM layout: [magic: 1 byte][temperature: int16][volume: int16]
static const uint8_t SETTINGS_MAGIC = 0xA5;
static const int ADDR_MAGIC       = 0;
static const int ADDR_TEMPERATURE = ADDR_MAGIC + 1;
static const int ADDR_VOLUME      = ADDR_TEMPERATURE + (int)sizeof(int16_t);

Settings SettingsStore::load() const {
  Settings settings;

  if (EEPROM.read(ADDR_MAGIC) != SETTINGS_MAGIC) {
    settings.temperature = DEFAULT_TEMPERATURE;
    settings.volume = DEFAULT_VOLUME;
    EEPROM.put(ADDR_TEMPERATURE, settings.temperature);
    EEPROM.put(ADDR_VOLUME, settings.volume);
    EEPROM.update(ADDR_MAGIC, SETTINGS_MAGIC);
    return settings;
  }

  EEPROM.get(ADDR_TEMPERATURE, settings.temperature);
  EEPROM.get(ADDR_VOLUME, settings.volume);
  settings.temperature = constrain(settings.temperature, VALUE_MIN, TEMPERATURE_MAX);
  settings.volume = constrain(settings.volume, VALUE_MIN, VOLUME_MAX);
  return settings;
}

// EEPROM.put only writes the bytes that changed.
void SettingsStore::saveTemperature(int16_t value) {
  EEPROM.put(ADDR_TEMPERATURE, value);
}

void SettingsStore::saveVolume(int16_t value) {
  EEPROM.put(ADDR_VOLUME, value);
}
