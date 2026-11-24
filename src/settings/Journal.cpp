// Journal.cpp: the ring of the last extractions in the EEPROM (see Journal.h).

#include "Journal.h"
#include <EEPROM.h>
#include <string.h>

static const int JOURNAL_ADDRESS = 24;   // 24 + 1 + 62 entries of 16 bytes = 1017: inside the first 1024 bytes of the EEPROM
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

uint8_t Journal::add(JournalEntry entry) const {
  JournalEntry last;
  entry.sequence = read(0, last) ? (uint8_t)(last.sequence + 1) : 0;
  const uint8_t slot = nextSlot();
  EEPROM.put(slotAddress(slot), entry);                                       // only the bytes that changed are written
  EEPROM.update(JOURNAL_ADDRESS, (uint8_t)((slot + 1) % JOURNAL_COUNT));      // after the entry: a power cut loses the entry only
  return entry.sequence;
}

void Journal::setLastCal(uint8_t cal) const {
  JournalEntry last;
  if (read(0, last)) {
    const uint8_t slot = (uint8_t)((nextSlot() + JOURNAL_COUNT - 1) % JOURNAL_COUNT);
    EEPROM.update(slotAddress(slot) + (int)offsetof(JournalEntry, cal), cal);
  }
}

// ------------------------------------------------------------
// The curves

static int curveSlotAddress(uint8_t slot) {
  return CURVE_ADDRESS + 1 + slot * CURVE_SLOT_SIZE;
}

// The curve ring is where the data flash is long enough for it (the test EEPROM and the R4 are; a smaller one has no curves)
static bool curvesFit() {
  return EEPROM.length() >= CURVE_ADDRESS + 1 + CURVE_COUNT * CURVE_SLOT_SIZE;
}

static uint8_t nextCurveSlot() {
  const uint8_t slot = EEPROM.read(CURVE_ADDRESS);
  return (slot < CURVE_COUNT) ? slot : 0;
}

struct CurveChunk { uint8_t bytes[CURVE_CHUNK]; };
struct CurveHeader { uint8_t sequence; uint8_t count; };

void Journal::beginCurve(uint8_t sequence, const uint8_t *samples, uint16_t count) {
  if (!curvesFit() || count == 0 || count > CURVE_SAMPLES) {
    stage_ = STAGE_NONE;
    return;
  }
  slot_ = nextCurveSlot();
  sequence_ = sequence;
  samples_ = samples;
  count_ = count;
  written_ = 0;
  stage_ = STAGE_ERASE_HEADER;
}

// The order of the writes: the header of the slot is blanked first (a power cut leaves no half curve that looks good), then the
// samples, then the real header, then the position of the ring (a power cut loses the curve only).
bool Journal::stepCurve() {
  const int address = curveSlotAddress(slot_);
  switch (stage_) {
    case STAGE_ERASE_HEADER: {
      const CurveHeader blank = {0xFF, 0xFF};
      EEPROM.put(address, blank);
      stage_ = STAGE_SAMPLES;
      return true;
    }
    case STAGE_SAMPLES: {
      CurveChunk chunk;
      memset(&chunk, 0, sizeof(chunk));
      const uint16_t left = 2 * count_ - written_;
      memcpy(chunk.bytes, samples_ + written_, left < CURVE_CHUNK ? left : CURVE_CHUNK);
      EEPROM.put(address + 2 + written_, chunk);
      written_ += CURVE_CHUNK;
      if (written_ >= 2 * count_) {
        stage_ = STAGE_HEADER;
      }
      return true;
    }
    case STAGE_HEADER: {
      const CurveHeader header = {sequence_, (uint8_t)count_};
      EEPROM.put(address, header);
      EEPROM.update(CURVE_ADDRESS, (uint8_t)((slot_ + 1) % CURVE_COUNT));
      stage_ = STAGE_NONE;
      return false;
    }
    default:
      return false;
  }
}

bool Journal::readCurve(uint8_t age, uint8_t &sequence, uint8_t *samples, uint16_t &count) const {
  if (!curvesFit() || age >= CURVE_COUNT) {
    return false;
  }
  const uint8_t slot = (uint8_t)((nextCurveSlot() + 2 * CURVE_COUNT - 1 - age) % CURVE_COUNT);
  const int address = curveSlotAddress(slot);
  CurveHeader header;
  EEPROM.get(address, header);
  if (header.count == 0 || header.count > CURVE_SAMPLES) {
    return false;   // blank or being written
  }
  sequence = header.sequence;
  count = header.count;
  for (uint16_t i = 0; i < 2 * count; i++) {
    samples[i] = EEPROM.read(address + 2 + i);
  }
  return true;
}
