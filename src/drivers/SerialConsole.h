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
};
