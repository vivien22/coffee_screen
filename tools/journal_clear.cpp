// journal_clear: erases the journal of the last extractions and their flow curves (EEPROM addresses 24 to 8191) and nothing else: the
// settings (addresses 0 to 23) are not touched. Flash it with: pio run -e journal_clear -t upload, then flash the machine again:
// pio run -t upload. Needed once when the layout of the journal or of the curves changes (the old ones would be misread).
#include <Arduino.h>
#include <EEPROM.h>

void setup() {
  Serial.begin(115200);
  const uint32_t start = millis();
  while (!Serial && millis() - start < 3000) {
    delay(10);   // the USB serial of the R4: the message below is seen only if a monitor is open
  }
  delay(300);
  for (int address = 24; address < EEPROM.length(); address++) {
    EEPROM.update(address, 0xFF);   // blank: an empty slot
  }
  Serial.println(F("JOURNAL CLEARED"));
}

void loop() {}
