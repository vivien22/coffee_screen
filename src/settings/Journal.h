/*
  Journal: the last JOURNAL_COUNT extractions, kept in the EEPROM (a ring: the oldest one is overwritten). One entry per
  AUTO extraction (the MANUAL ones are not kept), written when its values are final: how it ended and why, the volumes counted by the flow sensors, the
  settings it used and the machine state. It is read with the journal_dump environment (see platformio.ini), which prints the
  entries over the serial line: nothing in the machine itself shows or uses it.

  EEPROM layout, from address 128: [next slot: 1 byte][JOURNAL_COUNT entries of 25 bytes]. A blank (0xFF) or unknown slot is
  empty. The addresses 0..31 are the settings (Settings.cpp).
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

// 25 bytes, written as they are (the floats are kept as they are: no conversion in the machine)
struct JournalEntry {
  uint8_t reason;        // EndReason
  uint8_t phase;         // ExtractionState it was in when it ended (PREINFUSION, PREINFUSION_WAIT, RUNNING)
  uint8_t target;        // target volume (ml) of the settings
  uint8_t preinfusion;   // pre-infusion volume (ml) of the settings
  uint8_t wait;          // wait time (s) of the settings
  uint8_t duration;      // duration of the extraction (s), 255 at most
  uint8_t cup;           // final volume shown (ml): In - Out - loss, 255 at most
  uint8_t temperature;   // temperature shown at the end (degrees C)
  uint8_t sequence;      // number of the extraction, from 0, wraps at 256: gives the order
  float inMl;            // water counted by the In sensor (ml)
  float outMl;           // water counted by the Out sensor (ml)
  float loss;            // the loss (CAL) used (ml)
  float delay;           // the delay of the stop before the target (s of flow)
};

static const uint8_t JOURNAL_COUNT = 10;

class Journal {
  public:
    // Writes an entry (the sequence is given here): the next slot of the ring.
    void add(JournalEntry entry) const;

    // age 0 = the last extraction, 1 = the one before... Returns false if there is none.
    bool read(uint8_t age, JournalEntry &entry) const;
};
