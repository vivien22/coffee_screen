// The journal of the last extractions (Journal.cpp): a ring of JOURNAL_COUNT entries in the EEPROM, the last one first.
#include <cstdio>
#include <cstring>
#include "EEPROM.h"
#include "src/settings/Journal.h"
#include "TestSupport.h"

static JournalEntry entry(uint8_t reason, float in) {
  JournalEntry e; memset(&e, 0, sizeof(e));
  e.reason = reason; e.phase = 3; e.target = 60; e.preinfusion = 40; e.wait = 5; e.duration = 27; e.cup = 12; e.temperature = 91;
  e.inMl = in; e.outMl = 54.1f; e.loss = 36.25f; e.delay = 0.37f;
  return e;
}

int main() {
  Journal j; JournalEntry e;
  // a blank EEPROM: nothing in the journal
  memset(EEPROM.mem, 0xFF, sizeof(EEPROM.mem));
  CHECK(!j.read(0, e) && !j.read(5, e) && !j.read(JOURNAL_COUNT, e));
  // one entry: all the fields come back, the sequence starts at 0
  j.add(entry(END_TARGET, 102.3f));
  CHECK(j.read(0, e) && e.reason == END_TARGET && e.phase == 3 && e.target == 60 && e.preinfusion == 40 && e.wait == 5);
  CHECK(e.duration == 27 && e.cup == 12 && e.temperature == 91 && e.sequence == 0);
  CHECK(e.inMl == 102.3f && e.outMl == 54.1f && e.loss == 36.25f && e.delay == 0.37f);
  CHECK(!j.read(1, e));                                                  // only one
  // three more: the last one first, the order is the one of the writes
  j.add(entry(END_USER, 10.0f)); j.add(entry(END_NO_WATER, 20.0f)); j.add(entry(END_TIMER, 30.0f));
  CHECK(j.read(0, e) && e.reason == END_TIMER && e.sequence == 3 && e.inMl == 30.0f);
  CHECK(j.read(1, e) && e.reason == END_NO_WATER && e.sequence == 2);
  CHECK(j.read(2, e) && e.reason == END_USER && e.sequence == 1);
  CHECK(j.read(3, e) && e.reason == END_TARGET && e.sequence == 0);
  CHECK(!j.read(4, e));
  // more than the ring holds: only the last JOURNAL_COUNT remain, the oldest ones are overwritten
  for (int i = 4; i < 25; i++) j.add(entry(END_USER, (float)i));
  for (int age = 0; age < JOURNAL_COUNT; age++) {
    CHECK(j.read(age, e) && e.sequence == (uint8_t)(24 - age) && e.inMl == (float)(24 - age));
  }
  CHECK(!j.read(JOURNAL_COUNT, e));
  // the sequence wraps at 256
  for (int i = 25; i < 300; i++) j.add(entry(END_USER, 0.0f));
  CHECK(j.read(0, e) && e.sequence == (uint8_t)299 && j.read(1, e) && e.sequence == (uint8_t)298);
  // a damaged position of the ring (out of range) starts again at the first slot: no crash, entries still read
  EEPROM.mem[128] = 77;
  j.add(entry(END_USER, 5.0f));
  CHECK(j.read(0, e) && e.inMl == 5.0f);
  // a slot of the ring that was never written (0xFF) is empty
  memset(EEPROM.mem, 0xFF, sizeof(EEPROM.mem)); j.add(entry(END_TARGET, 1.0f));
  CHECK(j.read(0, e) && !j.read(1, e) && !j.read(9, e));
  // the writes: one entry costs at most its own bytes and the position, and a second one the same
  EEPROM.writes = 0; j.add(entry(END_TARGET, 1.0f));
  CHECK(EEPROM.writes <= (int)sizeof(JournalEntry) + 1);
  printf(fails ? "%d FAIL\n" : "ALL OK\n", fails); return fails != 0;
}
