// SerialConsole.cpp: see SerialConsole.h

#include "SerialConsole.h"
#include "../settings/JournalFormat.h"
#include "Perf.h"
#include <stdio.h>

void SerialConsole::printCurve(uint8_t sequence, uint16_t count, const uint8_t *samples) {
  // The header is short: a small buffer (the longest one, "CURVE #255 500 ms 180 | in ", is 27 characters)
  char header[40];
  snprintf(header, sizeof(header), "CURVE #%u %lu ms %u | in ", (unsigned)sequence, (unsigned long)CURVE_SAMPLE_MS, (unsigned)count);
  Serial.print(header);
  for (uint16_t i = 0; i < count; i++) {
    if (i > 0) {
      Serial.print(",");
    }
    Serial.print((unsigned)samples[2 * i]);   // In, in tenths of ml
  }
  Serial.print(" | out ");
  for (uint16_t i = 0; i < count; i++) {
    if (i > 0) {
      Serial.print(",");
    }
    Serial.print((unsigned)samples[2 * i + 1]);   // Out, in tenths of ml
  }
  Serial.println("");
}

static void printJournal(const Journal &journal) {
  Serial.println("JOURNAL");

  JournalEntry entry;
  char line[160];
  uint8_t count = 0;
  for (uint8_t age = 0; age < JOURNAL_COUNT; age++) {
    if (!journal.read(age, entry)) {
      continue;   // an empty slot
    }
    count++;
    formatJournalEntry(entry, line, sizeof(line));
    Serial.println((const char *)line);
  }
  if (count == 0) {
    Serial.println("(no extraction in the journal)");
  }

  // The curves of the last extractions (the flow In and Out), the last one first: the journal keeps the curve of the last CURVE_COUNT
  uint8_t samples[CURVE_BYTES];
  uint16_t samplesCount;
  for (uint8_t age = 0; age < JOURNAL_COUNT && age < CURVE_COUNT; age++) {
    if (journal.read(age, entry) && journal.readCurve(entry.sequence, samples, samplesCount)) {
      SerialConsole::printCurve(entry.sequence, samplesCount, samples);
    }
  }

  Serial.println("END");
}

void SerialConsole::begin() {
  Serial.begin(115200);
}

void SerialConsole::update(const Journal &journal) {
  while (Serial.available() > 0) {
    const int command = Serial.read();
    if (command == 'j' || command == 'J') {
      printJournal(journal);
    } else if (command == 'p' || command == 'P') {
      char line[100];
      perfTakeLine(line, sizeof(line));   // the maxima start again from here
      Serial.println((const char *)line);
    }
    // any other character (a newline after the letter, noise) is ignored
  }
}
