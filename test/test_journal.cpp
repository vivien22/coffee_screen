// The journal of the last extractions (Journal.cpp): a ring of JOURNAL_COUNT (100) entries of 9 bytes in the EEPROM, the last one first.
#include <cstdio>
#include <cstring>
#include "EEPROM.h"
#include "src/settings/Journal.h"
#include "TestSupport.h"

static JournalEntry entry(uint8_t reason, uint16_t in) {
  JournalEntry e; memset(&e, 0, sizeof(e));
  e.reason = reason; e.phase = 3; e.target = 60; e.preinfusion = 40; e.duration = 27; e.inTenths = in; e.outTenths = 541; e.loss4 = 145;
  return e;
}

int main() {
  Journal j; JournalEntry e;
  CHECK(sizeof(JournalEntry) == 9 && JOURNAL_COUNT == 100);
  // a blank EEPROM: nothing in the journal
  memset(EEPROM.mem, 0xFF, sizeof(EEPROM.mem));
  CHECK(!j.read(0, e) && !j.read(5, e) && !j.read(JOURNAL_COUNT, e));
  // one entry: all the fields come back
  j.add(entry(END_TARGET, 1023));
  CHECK(j.read(0, e) && e.reason == END_TARGET && e.phase == 3 && e.target == 60 && e.preinfusion == 40 && e.duration == 27);
  CHECK(e.inTenths == 1023 && e.outTenths == 541 && e.loss4 == 145);
  CHECK(!j.read(1, e));                                                  // only one
  // three more: the last one first, the order is the one of the writes
  j.add(entry(END_USER, 100)); j.add(entry(END_NO_WATER, 200)); j.add(entry(END_MAX_TIME, 300));
  CHECK(j.read(0, e) && e.reason == END_MAX_TIME && e.inTenths == 300);
  CHECK(j.read(1, e) && e.reason == END_NO_WATER && e.inTenths == 200);
  CHECK(j.read(2, e) && e.reason == END_USER && e.inTenths == 100);
  CHECK(j.read(3, e) && e.reason == END_TARGET && e.inTenths == 1023);
  CHECK(!j.read(4, e));
  // more than the ring holds: only the last JOURNAL_COUNT remain, the oldest ones are overwritten
  for (int i = 4; i < 250; i++) j.add(entry(END_USER, (uint16_t)i));
  for (int age = 0; age < JOURNAL_COUNT; age++) {
    CHECK(j.read(age, e) && e.inTenths == (uint16_t)(249 - age));
  }
  CHECK(!j.read(JOURNAL_COUNT, e));
  // the 100 entries and their position are inside the EEPROM of the Nano R3 (1024 bytes)
  CHECK(100 + 1 + JOURNAL_COUNT * (int)sizeof(JournalEntry) <= 1024);
  // a damaged position of the ring (out of range) starts again at the first slot: no crash, the new entry is the last one
  EEPROM.mem[100] = 177;
  j.add(entry(END_USER, 5));
  CHECK(j.read(0, e) && e.inTenths == 5);
  // a slot of the ring that was never written (0xFF) is empty
  memset(EEPROM.mem, 0xFF, sizeof(EEPROM.mem)); j.add(entry(END_TARGET, 1));
  CHECK(j.read(0, e) && !j.read(1, e) && !j.read(99, e));
  // the writes: an entry costs at most its own bytes and the position
  EEPROM.writes = 0; j.add(entry(END_TARGET, 1));
  CHECK(EEPROM.writes <= (int)sizeof(JournalEntry) + 1);
  printf(fails ? "%d FAIL\n" : "ALL OK\n", fails); return fails != 0;
}
