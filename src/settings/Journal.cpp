// Journal.cpp: the ring of the last extractions in the EEPROM (see Journal.h).

#include "Journal.h"
#include "Flash.h"
#include <EEPROM.h>
#include <string.h>

static const int JOURNAL_ADDRESS = 24;   // the settings are at 0..21; the layout of the whole EEPROM is in Journal.h
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

// The entry and the position of the ring are in the same block of the data flash: ONE block operation (the next slot after the entry
// in the same operation: a power cut during it can lose the block, which no order of the bytes changes)
uint8_t Journal::add(JournalEntry entry) const {
  JournalEntry last;
  entry.sequence = read(0, last) ? (uint8_t)(last.sequence + 1) : 0;
  const uint8_t slot = nextSlot();
  const uint8_t next = (uint8_t)((slot + 1) % JOURNAL_COUNT);
  const FlashRange ranges[] = {{slotAddress(slot), &entry, (uint16_t)ENTRY_SIZE}, {JOURNAL_ADDRESS, &next, 1}};
  flashWrite(ranges, 2);
  return entry.sequence;
}

void Journal::setLastCal(uint8_t cal) const {
  JournalEntry last;
  if (read(0, last)) {
    const uint8_t slot = (uint8_t)((nextSlot() + JOURNAL_COUNT - 1) % JOURNAL_COUNT);
    flashWrite(slotAddress(slot) + (int)offsetof(JournalEntry, cal), &cal, 1);
  }
}

// ------------------------------------------------------------
// The curves

// The curve ring is where the data flash is long enough for it (the test EEPROM and the R4 are; a smaller one has no curves)
static bool curvesFit() {
  return EEPROM.length() >= CURVE_ADDRESS + CURVE_COUNT * CURVE_SLOT_SIZE;
}

static int curveAddress(uint8_t sequence) {
  return CURVE_ADDRESS + (sequence % CURVE_COUNT) * CURVE_SLOT_SIZE;
}

// CRC-8 (polynomial 0x07), one byte at a time: the checksum of a curve covers its sequence number, its count and its samples
static uint8_t crc8(uint8_t crc, uint8_t byte) {
  crc ^= byte;
  for (uint8_t bit = 0; bit < 8; bit++) {
    crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
  }
  return crc;
}

static uint8_t curveChecksum(uint8_t sequence, uint8_t count, const uint8_t *samples) {
  uint8_t crc = crc8(crc8(0, sequence), count);
  for (uint16_t i = 0; i < 2 * (uint16_t)count; i++) {
    crc = crc8(crc, samples[i]);
  }
  return crc;
}

void Journal::beginCurve(uint8_t sequence, const uint8_t *samples, uint16_t count) {
  if (!curvesFit() || count == 0 || count > CURVE_SAMPLES) {
    stage_ = STAGE_NONE;
    return;
  }
  address_ = curveAddress(sequence);
  samples_ = samples;
  count_ = count;
  header_[0] = sequence;
  header_[1] = (uint8_t)count;
  header_[2] = curveChecksum(sequence, (uint8_t)count, samples);

  // The slot may span two blocks of the flash. The block that holds the first byte of the header is written LAST: the curve is valid
  // only when everything is there (a part written alone does not match the checksum, or the sequence of the slot).
  const int end = address_ + CURVE_HEADER_SIZE + 2 * (int)count;
  boundary_ = (address_ / FLASH_BLOCK_BYTES + 1) * FLASH_BLOCK_BYTES;
  stage_ = (end > boundary_) ? STAGE_SECOND_BLOCK : STAGE_FIRST_BLOCK;
}

// The bytes of the slot (header, then samples) that are at the addresses from..to-1: all in one block, so ONE block operation
void Journal::writePart(int from, int to) const {
  FlashRange ranges[2];
  uint8_t used = 0;

  const int headerEnd = address_ + CURVE_HEADER_SIZE;
  const int headerFrom = from > address_ ? from : address_;
  const int headerTo = to < headerEnd ? to : headerEnd;
  if (headerFrom < headerTo) {
    ranges[used++] = {headerFrom, header_ + (headerFrom - address_), (uint16_t)(headerTo - headerFrom)};
  }

  const int samplesEnd = headerEnd + 2 * (int)count_;
  const int samplesFrom = from > headerEnd ? from : headerEnd;
  const int samplesTo = to < samplesEnd ? to : samplesEnd;
  if (samplesFrom < samplesTo) {
    ranges[used++] = {samplesFrom, samples_ + (samplesFrom - headerEnd), (uint16_t)(samplesTo - samplesFrom)};
  }

  flashWrite(ranges, used);
}

bool Journal::stepCurve() {
  const int end = address_ + CURVE_HEADER_SIZE + 2 * (int)count_;
  switch (stage_) {
    case STAGE_SECOND_BLOCK:
      writePart(boundary_, end);
      stage_ = STAGE_FIRST_BLOCK;
      return true;
    case STAGE_FIRST_BLOCK:
      writePart(address_, end < boundary_ ? end : boundary_);
      stage_ = STAGE_NONE;
      return false;
    default:
      return false;
  }
}

bool Journal::readCurve(uint8_t sequence, uint8_t *samples, uint16_t &count) const {
  if (!curvesFit()) {
    return false;
  }
  const int address = curveAddress(sequence);
  const uint8_t headerSequence = EEPROM.read(address);
  const uint8_t headerCount = EEPROM.read(address + 1);
  const uint8_t headerChecksum = EEPROM.read(address + 2);
  if (headerSequence != sequence || headerCount == 0 || headerCount > CURVE_SAMPLES) {
    return false;   // another extraction, blank, or the layout of an older version
  }
  for (uint16_t i = 0; i < 2 * (uint16_t)headerCount; i++) {
    samples[i] = EEPROM.read(address + CURVE_HEADER_SIZE + i);
  }
  if (curveChecksum(sequence, headerCount, samples) != headerChecksum) {
    return false;   // half written, or damaged
  }
  count = headerCount;
  return true;
}
