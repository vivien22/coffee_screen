// Journal.cpp: the ring of the last extractions in the EEPROM (see Journal.h).

#include "Journal.h"
#include <EEPROM.h>

static const int JOURNAL_ADDRESS = 100;   // 100 + 1 + 100 entries of 9 bytes = 1001: inside the 1024 bytes of the Nano R3
static const int ENTRY_SIZE = (int)sizeof(JournalEntry);

static int slotAddress(uint8_t slot) {
  return JOURNAL_ADDRESS + 1 + slot * ENTRY_SIZE;
}

// The slot that comes next; a blank or out of range value means 0
static uint8_t nextSlot() {
  const uint8_t slot = EEPROM.read(JOURNAL_ADDRESS);
  return (slot < JOURNAL_COUNT) ? slot : 0;
}

bool Journal::read(uint8_t age, JournalEntry &entry) const {
  if (age >= JOURNAL_COUNT) {
    return false;
  }
  const uint8_t slot = (uint8_t)((nextSlot() + 2 * JOURNAL_COUNT - 1 - age) % JOURNAL_COUNT);
  EEPROM.get(slotAddress(slot), entry);
  return entry.reason >= END_TARGET && entry.reason <= END_TIMER;   // a blank slot (0xFF) is empty
}

void Journal::add(JournalEntry entry) const {
  const uint8_t slot = nextSlot();
  EEPROM.put(slotAddress(slot), entry);                                       // only the bytes that changed are written
  EEPROM.update(JOURNAL_ADDRESS, (uint8_t)((slot + 1) % JOURNAL_COUNT));      // after the entry: a power cut loses the entry only
}
