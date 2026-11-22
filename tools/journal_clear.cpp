// journal_clear: erases the journal of the last extractions (EEPROM addresses 24 to 1023) and nothing else: the settings (addresses
// 0 to 19) are not touched. Flash it with: pio run -e journal_clear -t upload, then flash the machine again:
// pio run -e nanoatmega328new -t upload. Needed once when the layout of a journal entry changes (the old entries would be misread).
#include <Arduino.h>
#include <EEPROM.h>

void setup() {
  Serial.begin(115200);
  delay(300);
  for (int address = 24; address < 1024; address++) {
    EEPROM.update(address, 0xFF);   // blank: an empty slot
  }
  Serial.println(F("JOURNAL CLEARED"));
}

void loop() {}
