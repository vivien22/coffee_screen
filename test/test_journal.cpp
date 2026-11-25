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
  // the writes: an entry costs at most its own bytes and the position, in ONE block operation of the data flash
  EEPROM.writes = 0; EEPROM.blockOps = 0; j.add(entry(END_TARGET, 1));
  CHECK(EEPROM.writes <= (int)sizeof(JournalEntry) + 1 && EEPROM.blockOps == 1);
  EEPROM.blockOps = 0; j.setLastCal(30); CHECK(EEPROM.blockOps == 1 && j.read(0, e) && e.cal == 30);   // the weight of CAL: one byte, one operation
  j.setLastCal(30); CHECK(EEPROM.blockOps == 1);                                                      // the same weight: nothing written

  // ---- The curves: the flow In and Out of the last CURVE_COUNT extractions, one slot each (sequence % CURVE_COUNT) after the entries
  memset(EEPROM.mem, 0xFF, sizeof(EEPROM.mem));
  uint8_t samples[CURVE_BYTES], back[CURVE_BYTES]; uint16_t n;
  CHECK(!j.readCurve(0, back, n) && !j.readCurve(7, back, n) && !j.readCurve(255, back, n));   // a blank EEPROM: none
  CHECK(CURVE_ADDRESS == 24 + 1 + JOURNAL_COUNT * (int)sizeof(JournalEntry));                  // right after the entries
  CHECK(CURVE_ADDRESS == 825 && CURVE_COUNT == 20 && CURVE_SLOT_SIZE == 363);                  // the map of Journal.h
  CHECK(CURVE_ADDRESS + CURVE_COUNT * CURVE_SLOT_SIZE <= 8192);                                // inside the 8 KB of the R4
  auto fill = [&](uint8_t seed, uint16_t count) { for (uint16_t i = 0; i < 2 * count; i++) samples[i] = (uint8_t)(seed + i); };
  auto blocksOf = [&](uint8_t sequence, uint16_t count) {   // the blocks of 1 KB that the slot of that curve touches
    const int address = CURVE_ADDRESS + (sequence % CURVE_COUNT) * CURVE_SLOT_SIZE, end = address + CURVE_HEADER_SIZE + 2 * count;
    return (end - 1) / 1024 - address / 1024 + 1; };
  // one curve: nothing is written by beginCurve(), the last step says there is no more; it is read back as it was, by its sequence
  fill(10, 84); EEPROM.blockOps = 0;
  j.beginCurve(7, samples, 84);
  CHECK(j.curvePending() && !j.readCurve(7, back, n) && EEPROM.blockOps == 0);
  int steps = 0;
  while (j.stepCurve()) steps++;
  CHECK(!j.curvePending() && steps + 1 == blocksOf(7, 84) && EEPROM.blockOps == blocksOf(7, 84));   // one block operation per step
  CHECK(j.readCurve(7, back, n) && n == 84 && memcmp(back, samples, 2 * 84) == 0);
  CHECK(!j.readCurve(8, back, n) && !j.readCurve(27, back, n) && !j.readCurve(6, back, n));         // other sequences: not there (27 has the same slot)
  CHECK(!j.stepCurve());                                                                            // nothing pending: nothing done
  // every slot, with the longest curve: at most two block operations, exactly the blocks it touches, and the header block comes last
  for (int sequence = 0; sequence < CURVE_COUNT; sequence++) {
    fill((uint8_t)(sequence * 3), CURVE_SAMPLES);
    EEPROM.blockOps = 0; j.beginCurve((uint8_t)sequence, samples, CURVE_SAMPLES);
    const int address = CURVE_ADDRESS + sequence * CURVE_SLOT_SIZE;
    int ops = 0;
    do {
      const bool more = j.stepCurve(); ops++;
      CHECK(more == j.curvePending());
      CHECK(EEPROM.blockOps == ops);
      if (more) CHECK(!j.readCurve((uint8_t)sequence, back, n) && EEPROM.mem[address] == 0xFF);      // the header is not written yet: no curve
    } while (j.curvePending());
    CHECK(ops <= 2 && ops == blocksOf((uint8_t)sequence, CURVE_SAMPLES));
    CHECK(j.readCurve((uint8_t)sequence, back, n) && n == CURVE_SAMPLES && memcmp(back, samples, CURVE_BYTES) == 0);
  }
  // the same curve again: nothing changes, nothing is written
  EEPROM.blockOps = 0; j.beginCurve(5, samples, CURVE_SAMPLES); fill(15, CURVE_SAMPLES); j.beginCurve(5, samples, CURVE_SAMPLES); j.flushCurve();
  CHECK(EEPROM.blockOps == 0 && j.readCurve(5, back, n));
  // a shorter curve in the same slot (an older curve with the same slot number, 20 extractions before): replaced, with the new header
  fill(77, 40); j.beginCurve(25, samples, 40); j.flushCurve();
  CHECK(j.readCurve(25, back, n) && n == 40 && back[0] == 77 && !j.readCurve(5, back, n));
  // more of them than the ring holds: only the last CURVE_COUNT remain
  for (int i = 0; i < 40; i++) { fill((uint8_t)i, CURVE_SAMPLES); j.beginCurve((uint8_t)(100 + i), samples, CURVE_SAMPLES); j.flushCurve(); }
  for (int age = 0; age < CURVE_COUNT; age++) {
    CHECK(j.readCurve((uint8_t)(139 - age), back, n) && n == CURVE_SAMPLES && back[0] == (uint8_t)(39 - age) && back[CURVE_BYTES - 1] == (uint8_t)(39 - age + CURVE_BYTES - 1));
  }
  CHECK(!j.readCurve(119, back, n) && !j.readCurve(99, back, n));
  // a power cut in the middle of a curve whose slot has two blocks (the second one is written first): the old curve of the slot is not
  // valid any more, the new one is not there, the others stay
  fill(200, CURVE_SAMPLES); j.beginCurve(140, samples, CURVE_SAMPLES);                            // slot 0: the same as 120
  CHECK(j.stepCurve());                                                                          // the second block only
  CHECK(!j.readCurve(140, back, n) && !j.readCurve(120, back, n) && j.readCurve(139, back, n) && j.readCurve(121, back, n));
  j.flushCurve();
  CHECK(j.readCurve(140, back, n) && n == CURVE_SAMPLES && back[0] == 200);
  // damaged curves are not shown: a wrong sample, a wrong checksum, another sequence number, a count that is too large or zero
  fill(1, 50); j.beginCurve(3, samples, 50); j.flushCurve();
  const int a3 = CURVE_ADDRESS + 3 * CURVE_SLOT_SIZE;
  CHECK(j.readCurve(3, back, n));
  EEPROM.mem[a3 + 3 + 10] ^= 1; CHECK(!j.readCurve(3, back, n)); EEPROM.mem[a3 + 3 + 10] ^= 1; CHECK(j.readCurve(3, back, n));      // a sample
  EEPROM.mem[a3 + 2] ^= 0x10; CHECK(!j.readCurve(3, back, n)); EEPROM.mem[a3 + 2] ^= 0x10; CHECK(j.readCurve(3, back, n));            // the checksum
  EEPROM.mem[a3 + 1] = CURVE_SAMPLES + 1; CHECK(!j.readCurve(3, back, n)); EEPROM.mem[a3 + 1] = 0; CHECK(!j.readCurve(3, back, n)); EEPROM.mem[a3 + 1] = 50; CHECK(j.readCurve(3, back, n));   // the count
  EEPROM.mem[a3] = 23; CHECK(!j.readCurve(3, back, n) && !j.readCurve(23, back, n)); EEPROM.mem[a3] = 3; CHECK(j.readCurve(3, back, n));   // another sequence (23 is the same slot, with the checksum of 3)
  // the layout of an older version (a next slot byte, slots of 362 bytes) or any other data: not a curve
  for (int i = CURVE_ADDRESS; i < 8192; i++) EEPROM.mem[i] = (uint8_t)(i * 7 + 3);
  { int shown = 0; for (int sequence = 0; sequence < 256; sequence++) shown += j.readCurve((uint8_t)sequence, back, n); CHECK(shown <= 1); }   // (1 in 256 can pass by chance)
  memset(EEPROM.mem + CURVE_ADDRESS, 0, 8192 - CURVE_ADDRESS);
  { int shown = 0; for (int sequence = 0; sequence < 256; sequence++) shown += j.readCurve((uint8_t)sequence, back, n); CHECK(shown == 0); }
  // nothing to write (no sample), too many samples: no crash, nothing pending
  j.beginCurve(1, samples, 0); CHECK(!j.curvePending());
  j.beginCurve(1, samples, CURVE_SAMPLES + 1); CHECK(!j.curvePending());
  // the sequence wraps at 256 (slot 15 for 255, 0 for 0): both curves are there
  memset(EEPROM.mem, 0xFF, sizeof(EEPROM.mem));
  fill(9, 12); j.beginCurve(255, samples, 12); j.flushCurve(); fill(8, 12); j.beginCurve(0, samples, 12); j.flushCurve();
  CHECK(j.readCurve(255, back, n) && back[0] == 9 && j.readCurve(0, back, n) && back[0] == 8);
  // the entries of the journal are not touched by the curves
  memset(EEPROM.mem, 0xFF, sizeof(EEPROM.mem));
  j.add(entry(END_TARGET, 11)); fill(0, CURVE_SAMPLES); j.beginCurve(0, samples, CURVE_SAMPLES); j.flushCurve();
  CHECK(j.read(0, e) && e.inTenths == 11 && !j.read(1, e));

  printf(fails ? "%d FAIL\n" : "ALL OK\n", fails); return fails != 0;
}
