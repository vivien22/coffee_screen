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
