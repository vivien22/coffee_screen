// The serial console (SerialConsole.cpp) and the text of a journal entry (JournalFormat.cpp): the command j prints the journal in the
// format that tools/journal_view.py reads, and nothing else is printed. The stub Serial keeps what is sent to it (Serial.out) and
// gives what a test puts in Serial.in.
#include <cstdio>
#include <cstring>
#include <string>
#include "EEPROM.h"
#include "src/settings/Journal.h"
#include "src/settings/JournalFormat.h"
#include "src/drivers/SerialConsole.h"
#include "src/drivers/Perf.h"
#include "TestSupport.h"

static JournalEntry entry(uint8_t reason, uint8_t phase, uint8_t pre, uint8_t duration, uint16_t in, uint16_t out, uint8_t loss4,
                          uint8_t cup, uint8_t cal, uint8_t delay100, uint8_t t0, uint8_t t1) {
  JournalEntry e; memset(&e, 0, sizeof(e));
  e.reason = reason; e.phase = phase; e.target = 60; e.preinfusion = pre; e.wait = 5; e.duration = duration;
  e.inTenths = in; e.outTenths = out; e.loss4 = loss4; e.cup = cup; e.cal = cal; e.delay100 = delay100; e.tempStart = t0; e.tempEnd = t1;
  return e;
}

static std::string line(const JournalEntry &e) {
  char buffer[160];
  formatJournalEntry(e, buffer, sizeof(buffer));
  return buffer;
}

int main() {
  // ---- the text of one entry: the numbers of the extraction #4 of the R4 (loss 149 quarters = 37.25 ml, shown 37.3)
  CHECK(line(entry(END_TARGET, 3, 40, 40, 1504, 541, 149, 59, 42, 39, 90, 84)) ==
        "#0 TARGET  ended in RUN  tgt 60 pre 40 wait 5 | 40 s | in 150.4 out 54.1 net 96.3 loss 37.3 cup 59 cal 42 | delay 0.39 | 90 -> 84 C");
  // no CAL: no "cal" in the line; a stop during the pre-infusion; a loss of exactly 30 ml; the delay keeps two digits
  CHECK(line(entry(END_USER, 1, 5, 0, 14, 0, 120, 0, 0, 5, 18, 18)) ==
        "#0 USER  ended in PRE  tgt 60 pre 5 wait 5 | 0 s | in 1.4 out 0.0 net 1.4 loss 30.0 cup 0 | delay 0.05 | 18 -> 18 C");
  // more water out than in (noise of the sensors): a negative net; a wait phase; the other end reasons; an unknown one
  CHECK(line(entry(END_NO_WATER, 2, 40, 12, 100, 250, 0, 0, 0, 37, 60, 65)).find("net -15.0 loss 0.0") != std::string::npos);
  CHECK(line(entry(END_NO_WATER, 2, 40, 12, 100, 250, 0, 0, 0, 37, 60, 65)).find("NO_WATER  ended in WAIT") != std::string::npos);
  CHECK(line(entry(END_MAX_TIME, 3, 0, 120, 0, 0, 0, 0, 0, 0, 0, 0)).find("MAX_TIME  ended in RUN") != std::string::npos);
  CHECK(line(entry(END_TIMER, 3, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0)).find("TIMER  ended in RUN") != std::string::npos);
  CHECK(line(entry(9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)).find("? ") != std::string::npos);   // an unknown reason and phase: "?"
  // the largest values still fit in the buffer of 160 bytes, and a small buffer cuts the line without overrunning
  {
    JournalEntry big = entry(END_NO_WATER, 2, 255, 255, 65535, 65535, 255, 255, 255, 255, 255, 255);
    big.target = 255; big.wait = 255; big.sequence = 255;
    char buffer[160];
    CHECK(formatJournalEntry(big, buffer, sizeof(buffer)) < sizeof(buffer) - 1);
    char small[20];
    const size_t n = formatJournalEntry(big, small, sizeof(small));
    CHECK(n == sizeof(small) - 1 && strlen(small) == n);
  }

  // ---- the console: nothing is said unless the command j is received
  Journal j;
  memset(EEPROM.mem, 0xFF, sizeof(EEPROM.mem));
  SerialConsole console;
  Serial.in.clear(); Serial.out.clear();
  console.update(j);
  CHECK(Serial.out.empty());
  Serial.in = "x\r\n 42";
  console.update(j);
  CHECK(Serial.out.empty() && Serial.in.empty());      // other characters are read and ignored

  // an empty journal
  Serial.in = "j";
  console.update(j);
  CHECK(Serial.out == "JOURNAL\n(no extraction in the journal)\nEND\n");

  // two entries, the last one first; the capital letter and a newline work too; two commands give two answers
  j.add(entry(END_USER, 1, 5, 0, 14, 0, 120, 0, 0, 39, 18, 18));
  j.add(entry(END_TARGET, 3, 40, 40, 1504, 541, 149, 59, 42, 39, 90, 84));
  Serial.out.clear();
  Serial.in = "J\n";
  console.update(j);
  const std::string once = "JOURNAL\n"
    "#1 TARGET  ended in RUN  tgt 60 pre 40 wait 5 | 40 s | in 150.4 out 54.1 net 96.3 loss 37.3 cup 59 cal 42 | delay 0.39 | 90 -> 84 C\n"
    "#0 USER  ended in PRE  tgt 60 pre 5 wait 5 | 0 s | in 1.4 out 0.0 net 1.4 loss 30.0 cup 0 | delay 0.39 | 18 -> 18 C\n"
    "END\n";
  CHECK(Serial.out == once);
  Serial.out.clear();
  Serial.in = "jj";
  console.update(j);
  CHECK(Serial.out == once + once);

  // ---- the curve line, as the console prints it (SerialConsole::printCurve): the empty curve and the longest line cannot come
  // back from the journal (no curve without a sample, at most 180 samples), so they are checked here
  {
    uint8_t one[2 * 5] = {0, 0, 32, 4, 255, 0, 9, 10, 100, 1};
    Serial.out.clear();
    SerialConsole::printCurve(12, 5, one);
    CHECK(Serial.out == "CURVE #12 500 ms 5 | in 0,32,255,9,100 | out 0,4,0,10,1\n");
    // the longest line: 180 samples of 255 (In and Out), 3 digits each, no comma after the last one
    uint8_t full[CURVE_BYTES];
    memset(full, 255, sizeof(full));
    std::string list;
    for (int i = 0; i < CURVE_SAMPLES; i++) {
      list += (i ? "," : "");
      list += "255";
    }
    Serial.out.clear();
    SerialConsole::printCurve(255, CURVE_SAMPLES, full);
    CHECK(Serial.out == "CURVE #255 500 ms 180 | in " + list + " | out " + list + "\n");
    // no sample: "in " and " | out " with nothing between them (two spaces before the bar)
    Serial.out.clear();
    SerialConsole::printCurve(1, 0, one);
    CHECK(Serial.out == "CURVE #1 500 ms 0 | in  | out \n");
  }

  // ---- the curves in the answer of j: one line each after the entries, the last one first, before END
  {
    // the console: the curves of the entries, after the entries, the last one first
    uint8_t first[2 * 3] = {1, 2, 3, 4, 5, 6}, second[2 * 2] = {7, 8, 9, 10}, other[2 * 2] = {1, 1, 1, 1};
    j.beginCurve(0, first, 3); j.flushCurve();
    j.beginCurve(1, second, 2); j.flushCurve();
    j.beginCurve(9, other, 2); j.flushCurve();                 // a curve whose entry is not in the journal: not printed
    Serial.out.clear(); Serial.in = "j";
    console.update(j);
    const std::string head = once.substr(0, once.size() - 4);   // the entries, without END
    CHECK(Serial.out == head +
          "CURVE #1 500 ms 2 | in 7,9 | out 8,10\n"
          "CURVE #0 500 ms 3 | in 1,3,5 | out 2,4,6\n"
          "END\n");
    // a curve that does not match its checksum is not printed, nor is one of another sequence in the slot
    EEPROM.mem[CURVE_ADDRESS + 1 * CURVE_SLOT_SIZE + CURVE_HEADER_SIZE] ^= 1;
    Serial.out.clear(); Serial.in = "j";
    console.update(j);
    CHECK(Serial.out == head + "CURVE #0 500 ms 3 | in 1,3,5 | out 2,4,6\n" "END\n");
    EEPROM.mem[CURVE_ADDRESS + 1 * CURVE_SLOT_SIZE + CURVE_HEADER_SIZE] ^= 1;
    j.beginCurve(21, second, 2); j.flushCurve();               // the slot of the sequence 1 now holds another extraction
    Serial.out.clear(); Serial.in = "j";
    console.update(j);
    CHECK(Serial.out == head + "CURVE #0 500 ms 3 | in 1,3,5 | out 2,4,6\n" "END\n");
    // only the curves of the last CURVE_COUNT entries are looked for: 30 more entries, the curves of #0 and #1 are not printed any more,
    // the one of #21 (written above, its entry is the 11th) is
    for (int i = 0; i < 30; i++) j.add(entry(END_USER, 1, 5, 0, 14, 0, 120, 0, 0, 39, 18, 18));
    Serial.out.clear(); Serial.in = "j";
    console.update(j);
    CHECK(Serial.out.find("CURVE #0 ") == std::string::npos && Serial.out.find("CURVE #1 ") == std::string::npos);
    CHECK(Serial.out.find("CURVE #21 500 ms 2 | in 7,9 | out 8,10\n") != std::string::npos);
  }

  // ---- the command p: one line of the performance trace, then the maxima start again
  {
    perfTakeLine(nullptr, 0);   // nothing measured yet in this test: start from zero
    Serial.out.clear(); Serial.in = "p";
    console.update(j);
    CHECK(Serial.out == "PERF loop max 0.0 ms | screen max 0.0 ms | flash 0 ops max 0.0 ms\n");
    perfRecord(PERF_LOOP, 12345); perfRecord(PERF_LOOP, 800); perfRecord(PERF_SCREEN, 9849);
    perfRecord(PERF_FLASH, 45050); perfRecord(PERF_FLASH, 3000); perfRecord(PERF_FLASH, 100);
    Serial.out.clear(); Serial.in = "P";
    console.update(j);
    CHECK(Serial.out == "PERF loop max 12.3 ms | screen max 9.8 ms | flash 3 ops max 45.1 ms\n");
    Serial.out.clear(); Serial.in = "p";
    console.update(j);
    CHECK(Serial.out == "PERF loop max 0.0 ms | screen max 0.0 ms | flash 0 ops max 0.0 ms\n");   // reset by the first one
    char small[10];
    CHECK(perfTakeLine(small, sizeof(small)) == strlen(small) && strlen(small) == sizeof(small) - 1);   // a small buffer cuts the line
  }

  if (fails == 0) printf("test_serial_console: ALL OK\n");
  return fails ? 1 : 0;
}
