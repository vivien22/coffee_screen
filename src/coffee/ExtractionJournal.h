/*
  ExtractionJournal: what the journal of the last extractions (settings/Journal.h) gets from the AUTO extraction in progress: its line
  and the curve of its flow In and Out (FlowRecorder).

  The curve is recorded from the push. Once the extraction is over and its volumes are final, the machine gives its line (capture()). It
  is written to the data flash by write(), which the machine calls only when a block operation (some milliseconds) can delay nothing:
  never while the pump may have to stop. The curve follows, one piece per pass (stepCurve()). MANUAL extractions are not kept.
*/

#pragma once
#include <Arduino.h>
#include "../settings/Journal.h"
#include "FlowRecorder.h"

class ExtractionJournal {
  public:
    const Journal &journal() const { return journal_; }   // read by the serial console

    // An AUTO extraction starts (pressTime: the push), tempStart: the temperature shown then. The line of the previous extraction is
    // written first if it was not, and its curve finished: its buffer is used again.
    void start(uint32_t pressTime, uint8_t tempStart);

    // The extraction will not go to the journal (it became a MANUAL one)
    void forget();

    // Called on every pass: the flow In and Out (does nothing unless an extraction is recorded)
    void record(uint32_t now, float inMl, float outMl) { recorder_.update(now, inMl, outMl); }

    // The extraction started waits for its line
    bool waiting() const { return waiting_; }

    // The line of the extraction, its values final (the sequence, the temperature at the start and the weight are set here): the
    // recording of the curve stops, the line waits for write()
    void capture(const JournalEntry &entry);

    bool writePending() const { return writePending_; }

    // The line goes to the journal (one block operation of the data flash); its curve is written by the next stepCurve()
    void write();

    // The cup weight entered in CAL, in the line of the last extraction (written first if it was not)
    void setLastCal(uint8_t weight);

    bool curvePending() const { return journal_.curvePending(); }
    void stepCurve() { journal_.stepCurve(); }

  private:
    Journal journal_;
    FlowRecorder recorder_;
    JournalEntry entry_;
    uint8_t tempStart_ = 0;
    bool waiting_ = false;        // an AUTO extraction was started and is not in the journal yet
    bool writePending_ = false;   // its line is made (entry_) and waits for write()
};
