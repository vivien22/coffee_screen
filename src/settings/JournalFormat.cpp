// JournalFormat.cpp: see JournalFormat.h

#include "JournalFormat.h"
#include "../coffee/ExtractionController.h"   // the phases the entry can end in
#include <stdio.h>

static const char *reasonName(uint8_t reason) {
  switch (reason) {
    case END_TARGET:   return "TARGET";
    case END_USER:     return "USER";
    case END_NO_WATER: return "NO_WATER";
    case END_MAX_TIME: return "MAX_TIME";
    case END_TIMER:    return "TIMER";
    default:           return "?";
  }
}

static const char *phaseName(uint8_t phase) {
  switch (phase) {
    case EXTRACTION_PREINFUSION:      return "PRE";
    case EXTRACTION_PREINFUSION_WAIT: return "WAIT";
    case EXTRACTION_RUNNING:          return "RUN";
    default:                          return "?";
  }
}

size_t formatJournalEntry(const JournalEntry &entry, char *out, size_t size) {
  if (size == 0) {
    return 0;
  }

  // Copies of the fields (the entry is packed: no pointer to a field), volumes in tenths of ml
  const unsigned sequence = entry.sequence;
  const unsigned inTenths = entry.inTenths;
  const unsigned outTenths = entry.outTenths;
  const int net = (int)inTenths - (int)outTenths;
  const unsigned netAbs = (unsigned)(net < 0 ? -net : net);
  const unsigned lossTenths = ((unsigned)entry.loss4 * 10 + 2) / 4;   // the loss is kept in quarters of ml: rounded to a tenth

  size_t length = (size_t)snprintf(out, size, "#%u %s  ended in %s  tgt %u pre %u wait %u | %u s | in %u.%u out %u.%u net %s%u.%u loss %u.%u cup %u",
                                   sequence, reasonName(entry.reason), phaseName(entry.phase),
                                   (unsigned)entry.target, (unsigned)entry.preinfusion, (unsigned)entry.wait, (unsigned)entry.duration,
                                   inTenths / 10, inTenths % 10, outTenths / 10, outTenths % 10,
                                   net < 0 ? "-" : "", netAbs / 10, netAbs % 10,
                                   lossTenths / 10, lossTenths % 10, (unsigned)entry.cup);
  if (length >= size) {
    return size - 1;   // cut by snprintf
  }

  if (entry.cal != 0) {   // the cup weight entered in CAL (nothing if it was not)
    length += (size_t)snprintf(out + length, size - length, " cal %u", (unsigned)entry.cal);
    if (length >= size) {
      return size - 1;
    }
  }

  length += (size_t)snprintf(out + length, size - length, " | delay %u.%02u | %u -> %u C",
                             (unsigned)entry.delay100 / 100, (unsigned)entry.delay100 % 100,
                             (unsigned)entry.tempStart, (unsigned)entry.tempEnd);
  return length >= size ? size - 1 : length;
}

// A number and the separator that follows it, in out at the position at (never beyond size - 1)
static void putNumber(char *out, size_t size, size_t &at, unsigned value, char separator) {
  char digits[4];
  uint8_t count = 0;
  do {
    digits[count++] = (char)('0' + value % 10);
    value /= 10;
  } while (value && count < sizeof(digits));
  while (count && at + 2 < size) {
    out[at++] = digits[--count];
  }
  if (separator && at + 1 < size) {
    out[at++] = separator;
  }
}

size_t formatCurveLine(uint8_t sequence, uint16_t count, const uint8_t *samples, char *out, size_t size) {
  if (size == 0) {
    return 0;
  }
  size_t at = (size_t)snprintf(out, size, "CURVE #%u %lu ms %u | in ", (unsigned)sequence, (unsigned long)CURVE_SAMPLE_MS, (unsigned)count);
  if (at >= size) {
    return size - 1;
  }
  for (uint16_t i = 0; i < count; i++) {
    putNumber(out, size, at, samples[2 * i], i + 1 < count ? ',' : 0);
  }
  const char *middle = " | out ";
  for (uint8_t i = 0; middle[i] && at + 1 < size; i++) {
    out[at++] = middle[i];
  }
  for (uint16_t i = 0; i < count; i++) {
    putNumber(out, size, at, samples[2 * i + 1], i + 1 < count ? ',' : 0);
  }
  out[at] = 0;
  return at;
}
