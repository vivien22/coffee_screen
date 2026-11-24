// The journal of the last extractions (Journal.cpp): a ring of JOURNAL_COUNT (50) entries of 16 bytes in the EEPROM, the last one first.
#include <cstdio>
#include <cstring>
#include "EEPROM.h"
#include "src/settings/Journal.h"
#include "TestSupport.h"

static JournalEntry entry(uint8_t reason, uint16_t in) {
  JournalEntry e; memset(&e, 0, sizeof(e));
  e.reason = reason; e.phase = 3; e.target = 60; e.preinfusion = 40; e.wait = 5; e.duration = 27; e.inTenths = in; e.outTenths = 541;
  e.loss4 = 145; e.cup = 12; e.delay100 = 37; e.tempStart = 88; e.tempEnd = 91;
  return e;
}

int main() {
  Journal j; JournalEntry e;
  CHECK(sizeof(JournalEntry) == 16 && JOURNAL_COUNT == 50);
  // a blank EEPROM: nothing in the journal
  memset(EEPROM.mem, 0xFF, sizeof(EEPROM.mem));
  CHECK(!j.read(0, e) && !j.read(5, e) && !j.read(JOURNAL_COUNT, e));
  // one entry: all the fields come back, the sequence starts at 0
  j.add(entry(END_TARGET, 1023));
  CHECK(j.read(0, e) && e.reason == END_TARGET && e.phase == 3 && e.target == 60 && e.preinfusion == 40 && e.wait == 5 && e.duration == 27);
  CHECK(e.inTenths == 1023 && e.outTenths == 541 && e.loss4 == 145 && e.cup == 12 && e.delay100 == 37 && e.tempStart == 88 && e.tempEnd == 91 && e.sequence == 0);
  CHECK(e.cal == 0);                                                     // CAL not entered
  CHECK(!j.read(1, e));                                                  // only one
  // three more: the last one first, the order is the one of the writes
  j.add(entry(END_USER, 100)); j.add(entry(END_NO_WATER, 200)); j.add(entry(END_MAX_TIME, 300));
  CHECK(j.read(0, e) && e.reason == END_MAX_TIME && e.inTenths == 300 && e.sequence == 3);
  CHECK(j.read(1, e) && e.reason == END_NO_WATER && e.inTenths == 200 && e.sequence == 2);
  CHECK(j.read(2, e) && e.reason == END_USER && e.inTenths == 100 && e.sequence == 1);
  CHECK(j.read(3, e) && e.reason == END_TARGET && e.inTenths == 1023 && e.sequence == 0);
  CHECK(!j.read(4, e));
  // more than the ring holds: only the last JOURNAL_COUNT remain, the oldest ones are overwritten
  for (int i = 4; i < 250; i++) j.add(entry(END_USER, (uint16_t)i));
  for (int age = 0; age < JOURNAL_COUNT; age++) {
    CHECK(j.read(age, e) && e.inTenths == (uint16_t)(249 - age) && e.sequence == (uint8_t)(249 - age));
  }
  CHECK(!j.read(JOURNAL_COUNT, e));
  // the entries and their position are inside the first 1024 bytes of the EEPROM, after the settings (0..19)
  CHECK(24 + 1 + JOURNAL_COUNT * (int)sizeof(JournalEntry) <= 1024);
  // the sequence wraps at 256
  for (int i = 250; i < 300; i++) j.add(entry(END_USER, (uint16_t)i));
  CHECK(j.read(0, e) && e.sequence == (uint8_t)299 && j.read(1, e) && e.sequence == (uint8_t)298);
  // a damaged position of the ring (out of range) starts again at the first slot: no crash, the new entry is the last one
  EEPROM.mem[24] = 177;
  j.add(entry(END_USER, 5));
  CHECK(j.read(0, e) && e.inTenths == 5);
  // a slot of the ring that was never written (0xFF) is empty
  memset(EEPROM.mem, 0xFF, sizeof(EEPROM.mem)); j.add(entry(END_TARGET, 1));
  CHECK(j.read(0, e) && !j.read(1, e) && !j.read(JOURNAL_COUNT - 1, e));
  // the writes: an entry costs at most its own bytes and the position
  EEPROM.writes = 0; j.add(entry(END_TARGET, 1));
  CHECK(EEPROM.writes <= (int)sizeof(JournalEntry) + 1);

  // ---- The curves: the flow In and Out of the last CURVE_COUNT extractions, in their own ring after the entries
  memset(EEPROM.mem, 0xFF, sizeof(EEPROM.mem));
  uint8_t samples[CURVE_BYTES], back[CURVE_BYTES]; uint16_t n; uint8_t sequence;
  CHECK(!j.readCurve(0, sequence, back, n) && !j.readCurve(CURVE_COUNT, sequence, back, n));   // a blank EEPROM: none
  CHECK(CURVE_ADDRESS == 24 + 1 + JOURNAL_COUNT * (int)sizeof(JournalEntry));                  // right after the entries
  CHECK(CURVE_ADDRESS == 825 && CURVE_COUNT == 20 && CURVE_SLOT_SIZE == 362);                 // the map of Journal.h
  CHECK(CURVE_ADDRESS + 1 + CURVE_COUNT * CURVE_SLOT_SIZE <= 8192);                            // inside the 8 KB of the R4
  CHECK(CURVE_BYTES % CURVE_CHUNK == 0);
  auto fill = [&](uint8_t seed, uint16_t count) { for (uint16_t i = 0; i < 2 * count; i++) samples[i] = (uint8_t)(seed + i); };
  // one curve: written a piece at a time, the last piece says there is no more; it is read back as it was
  fill(10, 84);
  j.beginCurve(7, samples, 84);
  CHECK(j.curvePending() && !j.readCurve(0, sequence, back, n));                               // nothing to read before the end
  int steps = 0;
  while (j.stepCurve()) steps++;
  CHECK(!j.curvePending() && steps == 1 + (2 * 84 + CURVE_CHUNK - 1) / CURVE_CHUNK + 0);       // the blank header, the pieces, then the header: the last call returns false
  CHECK(j.readCurve(0, sequence, back, n) && sequence == 7 && n == 84 && memcmp(back, samples, 2 * 84) == 0);
  CHECK(!j.readCurve(1, sequence, back, n));
  CHECK(!j.stepCurve());                                                                       // nothing pending: nothing done
  // the writes of a curve of 84 samples: its bytes, the header and the position of the ring, no more
  fill(33, 84); EEPROM.writes = 0;
  j.beginCurve(8, samples, 84); j.flushCurve();
  CHECK(EEPROM.writes <= 2 * 84 + 2 * 2 + 1 + CURVE_CHUNK);
  // several curves: the last one first
  fill(1, 10); j.beginCurve(9, samples, 10); j.flushCurve();
  CHECK(j.readCurve(0, sequence, back, n) && sequence == 9 && n == 10 && back[0] == 1 && back[19] == 20);
  CHECK(j.readCurve(1, sequence, back, n) && sequence == 8 && n == 84 && back[0] == 33);
  CHECK(j.readCurve(2, sequence, back, n) && sequence == 7 && n == 84 && back[0] == 10);
  CHECK(!j.readCurve(3, sequence, back, n));
  // a full curve (CURVE_SAMPLES), and more of them than the ring holds: only the last CURVE_COUNT remain
  for (int i = 0; i < 40; i++) { fill((uint8_t)i, CURVE_SAMPLES); j.beginCurve((uint8_t)(100 + i), samples, CURVE_SAMPLES); j.flushCurve(); }
  for (int age = 0; age < CURVE_COUNT; age++) {
    CHECK(j.readCurve(age, sequence, back, n) && sequence == 139 - age && n == CURVE_SAMPLES && back[0] == (uint8_t)(39 - age) && back[CURVE_BYTES - 1] == (uint8_t)(39 - age + CURVE_BYTES - 1));
  }
  CHECK(!j.readCurve(CURVE_COUNT, sequence, back, n));
  // a power cut in the middle of a curve: the slot has no valid header, the curve is not there (the one before stays)
  fill(200, 50); j.beginCurve(150, samples, 50);
  for (int i = 0; i < 3; i++) j.stepCurve();                                                   // the blank header and two pieces
  CHECK(j.readCurve(0, sequence, back, n) && sequence == 139 && j.readCurve(CURVE_COUNT - 2, sequence, back, n));   // the slot of the cut is the oldest of the ring: the others stay
  j.flushCurve();
  CHECK(j.readCurve(0, sequence, back, n) && sequence == 150 && n == 50 && back[0] == 200);
  // nothing to write (no sample), too many samples, or a damaged position of the ring: no crash
  j.beginCurve(1, samples, 0); CHECK(!j.curvePending());
  j.beginCurve(1, samples, CURVE_SAMPLES + 1); CHECK(!j.curvePending());
  EEPROM.mem[CURVE_ADDRESS] = 200;
  fill(5, 4); j.beginCurve(77, samples, 4); j.flushCurve();
  CHECK(j.readCurve(0, sequence, back, n) && sequence == 77 && n == 4);
  // the entries of the journal are not touched by the curves
  memset(EEPROM.mem, 0xFF, sizeof(EEPROM.mem));
  j.add(entry(END_TARGET, 11)); fill(0, CURVE_SAMPLES); j.beginCurve(0, samples, CURVE_SAMPLES); j.flushCurve();
  CHECK(j.read(0, e) && e.inTenths == 11 && !j.read(1, e));

  printf(fails ? "%d FAIL\n" : "ALL OK\n", fails); return fails != 0;
}
