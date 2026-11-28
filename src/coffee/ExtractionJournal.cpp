// ExtractionJournal.cpp: see ExtractionJournal.h

#include "ExtractionJournal.h"

void ExtractionJournal::start(uint32_t pressTime, uint8_t tempStart) {
  if (writePending_) {
    write();                     // (the previous extraction is normally written long before)
  }
  journal_.flushCurve();         // the previous curve is in the EEPROM before its buffer is used again
  recorder_.begin(pressTime);    // the flow In and Out is recorded until the journal gets the extraction
  tempStart_ = tempStart;
  waiting_ = true;
}

void ExtractionJournal::forget() {
  waiting_ = false;
  recorder_.stop();
}

void ExtractionJournal::capture(const JournalEntry &entry) {
  entry_ = entry;
  entry_.sequence = 0;            // given by the journal
  entry_.tempStart = tempStart_;
  entry_.cal = 0;                 // set later if the cup weight is entered in CAL
  recorder_.stop();
  waiting_ = false;
  writePending_ = true;
}

void ExtractionJournal::write() {
  writePending_ = false;
  const uint8_t sequence = journal_.add(entry_);
  journal_.beginCurve(sequence, recorder_.samples(), recorder_.count());   // written by the next passes (stepCurve())
}

void ExtractionJournal::setLastCal(uint8_t weight) {
  if (writePending_) {
    write();   // the line has to be in the journal to receive the weight (no extraction is going on here)
  }
  journal_.setLastCal(weight);
}
