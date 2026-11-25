// journal_clear: erases the journal of the last extractions and their flow curves (EEPROM addresses 24 to 8191) and nothing else: the
// settings (addresses 0 to 23) are not touched. Flash it with: pio run -e journal_clear -t upload, then flash the machine again:
// pio run -t upload. Needed once when the layout of the journal or of the curves changes (the old ones would be misread).
#include <Arduino.h>
#include <EEPROM.h>
#include "../src/settings/Flash.h"

// The data flash is erased and programmed one block of 1 KB at a time: the addresses 24 to 8191 are blanked block by block, one erase and
// program per block that holds something (the old way, EEPROM.update() byte by byte, was one block operation per byte: minutes).
static uint8_t blank[FLASH_BLOCK_BYTES];

void setup() {
  Serial.begin(115200);
  const uint32_t start = millis();
  while (!Serial && millis() - start < 3000) {
    delay(10);   // the USB serial of the R4: the message below is seen only if a monitor is open
  }
  delay(300);
  memset(blank, 0xFF, sizeof(blank));   // blank: an empty slot
  for (int address = 24; address < EEPROM.length(); ) {
    const int toBlockEnd = FLASH_BLOCK_BYTES - address % FLASH_BLOCK_BYTES;   // up to the end of the block: ONE operation
    const int size = min(toBlockEnd, EEPROM.length() - address);
    flashWrite(address, blank, (uint16_t)size);
    address += size;
  }
  Serial.println(F("JOURNAL CLEARED"));
}

void loop() {}
