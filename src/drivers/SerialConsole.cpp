// SerialConsole.cpp: see SerialConsole.h

#include "SerialConsole.h"
#include "../settings/JournalFormat.h"

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

  // The curves of the last extractions (the flow In and Out), the last one first
  static char curveLine[2048];
  uint8_t samples[CURVE_BYTES];
  uint8_t sequence;
  uint16_t samplesCount;
  for (uint8_t age = 0; age < CURVE_COUNT; age++) {
    if (journal.readCurve(age, sequence, samples, samplesCount)) {
      formatCurveLine(sequence, samplesCount, samples, curveLine, sizeof(curveLine));
      Serial.println((const char *)curveLine);
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
    }
    // any other character (a newline after the letter, noise) is ignored
  }
}
