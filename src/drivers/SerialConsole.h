/*
  SerialConsole: a few one-letter commands on the USB serial line of the machine (115200 baud), so that what is inside the
  machine can be read without flashing another program. The machine says nothing by itself: it only answers a command.

    j   prints the journal of the last extractions (the text that tools/journal_view.py and tools/journal_viewer.html read: "JOURNAL",
        one line per extraction, the last one first, then one "CURVE" line per extraction that has its flow curve, "END")

    p   prints one line of the performance trace (drivers/Perf.h): the longest pass of the loop, the longest drawing and the data flash
        block operations since the last p, e.g. "PERF loop max 12.3 ms | screen max 9.8 ms | flash 3 ops max 45.1 ms", then starts it again

  Reading never writes anything. Use tools/journal_view.py --port COM<n> for the charts.
*/

#pragma once
#include <Arduino.h>
#include "../settings/Journal.h"

class SerialConsole {
  public:
    // Opens the serial line.
    void begin();

    // Call on every loop pass: answers the commands received since the last pass.
    void update(const Journal &journal);

    // Prints one curve line ("CURVE #12 500 ms 84 | in 0,5,14,20 | out 0,0,0,2", see JournalFormat.h), piece by piece: no line buffer.
    // update() calls it for each curve of the journal; it is public because the journal never gives back an empty curve (the test uses it).
    static void printCurve(uint8_t sequence, uint16_t count, const uint8_t *samples);
};
