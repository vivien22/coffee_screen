// journal_dump: a small sketch that prints the journal of the last 100 AUTO extractions (EEPROM) over the serial line (115200 baud),
// then does nothing. It does not write anything. Flash it with: pio run -e journal_dump -t upload, read the serial line, then flash
// the machine again: pio run -e nanoatmega328new -t upload (the EEPROM is kept).
//
// One line per extraction, the last one first (the number is its age: 0 = the last one):
//   #0 TARGET  ended in RUN  tgt 60 pre 40 | 27 s | in 102.3 out 54.1 net 48.2 loss 36.3 cup 12
#include <Arduino.h>
#include <EEPROM.h>
#include "../src/settings/Journal.h"
#include "../src/coffee/ExtractionController.h"

static const __FlashStringHelper *reasonName(uint8_t reason) {
  switch (reason) {
    case END_TARGET:   return F("TARGET");
    case END_USER:     return F("USER");
    case END_NO_WATER: return F("NO_WATER");
    case END_MAX_TIME: return F("MAX_TIME");
    case END_TIMER:    return F("TIMER");
    default:           return F("?");
  }
}

static const __FlashStringHelper *phaseName(uint8_t phase) {
  switch (phase) {
    case EXTRACTION_PREINFUSION:      return F("PRE");
    case EXTRACTION_PREINFUSION_WAIT: return F("WAIT");
    case EXTRACTION_RUNNING:          return F("RUN");
    default:                          return F("?");
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println(F("JOURNAL"));

  const Journal journal;
  JournalEntry e;
  uint8_t count = 0;
  for (uint8_t age = 0; age < JOURNAL_COUNT; age++) {
    if (!journal.read(age, e)) {
      continue;
    }
    count++;
    const float in = e.inTenths / 10.0f;
    const float out = e.outTenths / 10.0f;
    const float loss = e.loss4 / 4.0f;
    Serial.print('#'); Serial.print(age);
    Serial.print(' '); Serial.print(reasonName(e.reason));
    Serial.print(F("  ended in ")); Serial.print(phaseName(e.phase));
    Serial.print(F("  tgt ")); Serial.print(e.target);
    Serial.print(F(" pre ")); Serial.print(e.preinfusion);
    Serial.print(F(" | ")); Serial.print(e.duration); Serial.print(F(" s | in "));
    Serial.print(in, 1); Serial.print(F(" out ")); Serial.print(out, 1);
    Serial.print(F(" net ")); Serial.print(in - out, 1);
    Serial.print(F(" loss ")); Serial.print(loss, 1);
    Serial.print(F(" cup ")); Serial.println(in - out - loss, 1);
  }
  if (count == 0) {
    Serial.println(F("(no extraction in the journal)"));
  }
  Serial.println(F("END"));
}

void loop() {}
