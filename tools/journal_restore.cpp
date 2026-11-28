// journal_restore: writes a saved journal back into the EEPROM of the machine: the entries (with their numbers) and their flow curves,
// from tools/journal_restore_data.h (made by tools/journal_restore_gen.py from the text of the journal). The settings are not touched.
// Only into an EMPTY journal (after pio run -e journal_clear -t upload): it refuses otherwise. Flash it with:
// pio run -e journal_restore -t upload, then flash the machine again: pio run -t upload.
#include <Arduino.h>
#include "../src/settings/Journal.h"
#include "../src/coffee/ExtractionController.h"   // the phases (ExtractionState)

struct RestoredEntry {
  uint8_t reason, phase, target, preinfusion, wait, duration;
  uint16_t inTenths, outTenths;
  uint8_t loss, cup, delay100, tempStart, tempEnd, cal;
};
struct RestoredCurve {
  uint8_t sequence;
  uint16_t count;
  const uint8_t *samples;   // In, Out of each sample (tenths of ml)
};

#include "journal_restore_data.h"

void setup() {
  Serial.begin(115200);
  const uint32_t start = millis();
  while (!Serial && millis() - start < 3000) {
    delay(10);   // the USB serial of the R4: the messages are seen only if a monitor is open
  }
  delay(300);

  Journal journal;
  JournalEntry last;
  if (journal.read(0, last)) {
    Serial.println(F("JOURNAL NOT EMPTY: nothing written (flash journal_clear first)"));
    return;
  }

  for (const RestoredEntry &r : RESTORED_ENTRIES) {
    JournalEntry e;
    e.reason = r.reason;
    e.phase = r.phase;
    e.target = r.target;
    e.preinfusion = r.preinfusion;
    e.wait = r.wait;
    e.duration = r.duration;
    e.inTenths = r.inTenths;
    e.outTenths = r.outTenths;
    e.loss = r.loss;
    e.cup = r.cup;
    e.delay100 = r.delay100;
    e.tempStart = r.tempStart;
    e.tempEnd = r.tempEnd;
    e.cal = r.cal;
    journal.add(e);   // the numbers follow from 0, as in the saved journal
  }
  for (const RestoredCurve &c : RESTORED_CURVES) {
    journal.beginCurve(c.sequence, c.samples, c.count);
    journal.flushCurve();
  }
  Serial.print(F("JOURNAL RESTORED: "));
  Serial.print((unsigned)(sizeof(RESTORED_ENTRIES) / sizeof(RESTORED_ENTRIES[0])));
  Serial.print(F(" entries, "));
  Serial.print((unsigned)(sizeof(RESTORED_CURVES) / sizeof(RESTORED_CURVES[0])));
  Serial.println(F(" curves"));
}

void loop() {}
