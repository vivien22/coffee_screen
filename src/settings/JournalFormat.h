/*
  JournalFormat: one entry of the journal as one line of text, the format that tools/journal_view.py reads:

    #12 TARGET  ended in RUN  tgt 60 pre 40 wait 5 | 27 s | in 102.3 out 54.1 net 48.2 loss 36.3 cup 12 cal 10 | delay 0.37 | 88 -> 91 C

  (`cal` only when a weight was entered in CAL.) Plain C++ with integers only (no float printing: it needs a special link option
  on some cores): it is tested on the computer (test/test_serial_console.cpp) and used by the serial console on the board.
*/

#pragma once
#include <stddef.h>
#include "Journal.h"

// The curve of an extraction as one line, after the entries of the journal:
//
//   CURVE #12 500 ms 84 | in 0,5,14,20,... | out 0,0,0,2,...
//
// #12 is the sequence number of its entry, 500 the time between two samples, 84 the number of samples; then the water that went through
// the In sensor and through the Out sensor during each sample, in tenths of ml (the first sample ends one interval after the push).
// samples: in[0], out[0], in[1], out[1]... as FlowRecorder keeps them. The line is at most 2000 characters: give a buffer of 2048 bytes.
size_t formatCurveLine(uint8_t sequence, uint16_t count, const uint8_t *samples, char *out, size_t size);

// Writes the line (without a newline) in out, never more than size bytes, always ended by a 0. Returns its length.
// The line is at most about 150 characters: give a buffer of 160 bytes.
size_t formatJournalEntry(const JournalEntry &entry, char *out, size_t size);
