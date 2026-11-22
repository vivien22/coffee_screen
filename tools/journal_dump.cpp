// journal_dump: a small sketch that prints the journal of the last extractions (EEPROM) and the saved settings over the serial
// line (115200 baud), then does nothing. It does not write anything. Flash it with: pio run -e journal_dump -t upload,
// read the serial line, then flash the machine again: pio run -e nanoatmega328new -t upload (the EEPROM is kept).
//
// One line per extraction, the last one first:
//   #12 TARGET  RUN  AUTO tgt 60 pre 40 wait 5 | 27 s | in 102.3 out 54.1 net 48.2 | cup 12 loss 36.3 delay 0.37 | 91 C
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
    case EXTRACTION_MANUAL:           return F("MANUAL");
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
    Serial.print('#'); Serial.print(e.sequence);
    Serial.print(' '); Serial.print(reasonName(e.reason));
    Serial.print(F("  ended in ")); Serial.print(phaseName(e.phase));
    Serial.print((e.flags & 1) ? F("  MANUAL") : F("  AUTO"));
    Serial.print(F(" tgt ")); Serial.print(e.target);
    Serial.print(F(" pre ")); Serial.print(e.preinfusion);
    Serial.print(F(" wait ")); Serial.print(e.wait);
    Serial.print(F(" | ")); Serial.print(e.duration); Serial.print(F(" s | in "));
    Serial.print(e.inMl, 1); Serial.print(F(" out ")); Serial.print(e.outMl, 1);
    Serial.print(F(" net ")); Serial.print(e.inMl - e.outMl, 1);
    Serial.print(F(" | cup ")); Serial.print(e.cup);
    Serial.print(F(" loss ")); Serial.print(e.loss, 1);
    Serial.print(F(" delay ")); Serial.print(e.delay, 2);
    Serial.print(F(" | ")); Serial.print(e.temperature); Serial.println(F(" C"));
  }
  if (count == 0) {
    Serial.println(F("(no extraction in the journal)"));
  }
  Serial.println(F("END"));
}

void loop() {}
