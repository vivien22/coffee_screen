/*
  Journal: the last JOURNAL_COUNT (62) AUTO extractions, kept in the EEPROM (a ring: the oldest one is overwritten). One entry per
  extraction, written when its values are final: how it ended and why, the volumes counted by the flow sensors, the final volume,
  the loss, the delay of the stop, the temperatures and the settings it used. The weight of the cup entered in the CAL parameter
  is added to the entry afterwards (setLastCal), only if it was changed: otherwise the field stays 0. The MANUAL extractions are not kept. It is read
  over the USB serial line: the machine answers the letter j (drivers/SerialConsole.cpp). Nothing else in the machine shows or uses it.

  EEPROM layout, from address JOURNAL_ADDRESS (Journal.cpp): [next slot: 1 byte][JOURNAL_COUNT entries of 16 bytes]: 993 bytes of the
  the first 1024 bytes of the EEPROM, after the settings (addresses 0..19). A blank (0xFF) or unknown slot is empty.

  The curves (the flow In and Out of an extraction, see coffee/FlowRecorder.h) are kept apart, for the last CURVE_COUNT (14) extractions,
  from address CURVE_ADDRESS (1024, the data flash of the R4 holds 8 KB): [next slot: 1 byte][CURVE_COUNT slots of 2 + 2 x CURVE_SAMPLES
  bytes: the sequence number of the entry, the number of samples, the samples]. A curve belongs to the entry with the same sequence number.
  It is written in small pieces (stepCurve) so that the loop of the machine never waits for the flash.
*/

#pragma once
#include <Arduino.h>
#include "../Config.h"

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
static const uint8_t CURVE_COUNT = 14;
static const uint16_t CURVE_BYTES = 2 * CURVE_SAMPLES;         // In and Out of each sample (tenths of ml)
static const uint16_t CURVE_SLOT_SIZE = 2 + CURVE_BYTES;       // sequence, number of samples, samples
static const uint16_t CURVE_ADDRESS = 1024;
static const uint8_t CURVE_CHUNK = 32;                          // bytes written by one step (CURVE_BYTES is a multiple of it)

class Journal {
  public:
    // Writes an entry (the sequence is given here): the next slot of the ring. Returns the sequence.
    uint8_t add(JournalEntry entry) const;

    // The cup weight entered in CAL (validated, changed) goes to the last entry: the extraction CAL was offered for.
    void setLastCal(uint8_t cal) const;

    // age 0 = the last extraction, 1 = the one before... Returns false if there is none.
    bool read(uint8_t age, JournalEntry &entry) const;

    // ----- The curves -----
    // The curve of the extraction whose entry has this sequence number (count samples, In and Out of each, see FlowRecorder) goes to
    // the next slot of the curve ring. samples must stay as it is until stepCurve() has returned false (or flushCurve() was called).
    // Nothing is written here: stepCurve() does it, a piece at a time.
    void beginCurve(uint8_t sequence, const uint8_t *samples, uint16_t count);
    // Writes the next piece (at most CURVE_CHUNK bytes, one call = one write to the flash). Returns true while there is more to write.
    bool stepCurve();
    // Writes what remains, at once (before the samples are needed for another curve)
    void flushCurve() { while (stepCurve()) {} }
    bool curvePending() const { return stage_ != STAGE_NONE; }

    // The curve of age 0 (the last one), 1... into samples (CURVE_BYTES bytes). Returns false if there is none.
    bool readCurve(uint8_t age, uint8_t &sequence, uint8_t *samples, uint16_t &count) const;

  private:
    enum Stage : uint8_t { STAGE_NONE, STAGE_ERASE_HEADER, STAGE_SAMPLES, STAGE_HEADER };
    Stage stage_ = STAGE_NONE;
    uint8_t slot_ = 0;
    uint8_t sequence_ = 0;
    const uint8_t *samples_ = nullptr;
    uint16_t count_ = 0;
    uint16_t written_ = 0;   // bytes of the samples already written
};
