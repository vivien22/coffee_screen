/*
  Journal: the last JOURNAL_COUNT (62) AUTO extractions, kept in the EEPROM (a ring: the oldest one is overwritten). One entry per
  extraction, written when its values are final: how it ended and why, the volumes counted by the flow sensors, the final volume,
  the loss, the delay of the stop, the temperatures and the settings it used. The weight of the cup entered in the CAL parameter
  is added to the entry afterwards (setLastCal), only if it was changed: otherwise the field stays 0. The MANUAL extractions are not kept. It is read
  with the journal_dump environment (see platformio.ini), which prints the entries over the serial line: nothing in the machine
  itself shows or uses it.

  EEPROM layout, from address JOURNAL_ADDRESS (Journal.cpp): [next slot: 1 byte][JOURNAL_COUNT entries of 16 bytes]: 993 bytes of the
  1024 of the Nano R3, after the settings (addresses 0..19). A blank (0xFF) or unknown slot is empty.
*/

#pragma once
#include <Arduino.h>

// Why the extraction ended (the first reason wins: the end of a flush does not replace it)
enum EndReason : uint8_t {
  END_NONE = 0,
  END_TARGET,      // the target volume was reached (AUTO)
  END_USER,        // the start / stop button
  END_NO_WATER,    // the no-flow safety: no water entering the boiler
  END_MAX_TIME,    // the maximum duration of an extraction (DURATION_MAX)
  END_TIMER        // a timed phase elapsed (not used by the AUTO extractions: kept for the numbering)
};

// 16 bytes, written as they are (packed: the same size on every board)
struct __attribute__((packed)) JournalEntry {
  uint8_t reason : 4;    // EndReason
  uint8_t phase : 4;     // ExtractionState it was in when it ended (PREINFUSION, PREINFUSION_WAIT, RUNNING)
  uint8_t target;        // target volume (ml) of the settings
  uint8_t preinfusion;   // pre-infusion volume (ml) of the settings
  uint8_t wait;          // wait time (s) of the settings
  uint8_t duration;      // duration of the extraction (s)
  uint16_t inTenths;     // water counted by the In sensor, in tenths of ml
  uint16_t outTenths;    // water counted by the Out sensor, in tenths of ml
  uint8_t loss4;         // the loss (CAL) used, in quarters of ml
  uint8_t cup;           // final volume shown (ml): In - Out - loss
  uint8_t delay100;      // the delay of the stop before the target, in hundredths of a second of flow
  uint8_t tempStart;     // temperature shown at the start of the extraction (degrees C)
  uint8_t tempEnd;       // temperature shown when the entry was written (a few seconds after the end)
  uint8_t sequence;      // number of the extraction, from 0, wraps at 256: gives the order
  uint8_t cal;           // the cup weight (g = ml) entered in CAL and validated, 0 = not entered
};

static const uint8_t JOURNAL_COUNT = 62;

class Journal {
  public:
    // Writes an entry (the sequence is given here): the next slot of the ring.
    void add(JournalEntry entry) const;

    // The cup weight entered in CAL (validated, changed) goes to the last entry: the extraction CAL was offered for.
    void setLastCal(uint8_t cal) const;

    // age 0 = the last extraction, 1 = the one before... Returns false if there is none.
    bool read(uint8_t age, JournalEntry &entry) const;
};
