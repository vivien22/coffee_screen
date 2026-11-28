/*
  DigitRoll: the number of a field that changes during an extraction (the volume, the duration) rolls from the old one to the new one
  (see ROLL_VOLUME_MS in Config.h). It only keeps the time and gives the progress of each frame (0 to DISPLAY_ROLL_DONE - 1, then
  DISPLAY_ROLL_DONE at the end): the caller draws the number with it. A frame is due at most every ROLL_FRAME_MS.

  A new number that comes during a roll: the number of that roll is drawn as it is (finished), then the new roll starts.
*/

#pragma once
#include <stdint.h>

class DigitRoll {
  public:
    bool active() const { return active_; }         // the frames are being drawn
    bool requested() const { return requested_; }   // a new number came: the next drawing of the field starts the roll
    bool busy() const { return active_ || requested_; }

    // A new number came; shownBefore is the number of the roll in progress, if any (it will be drawn finished first)
    void request(int16_t shownBefore) {
      if (active_) {
        finishFirst_ = true;
        finishValue_ = shownBefore;
      }
      requested_ = true;
    }

    // The field is drawn for the new number: the roll starts (durationMs 0: no roll, the caller draws the number as it is, active()
    // is false). Returns true if the number of the roll that was in progress must be drawn finished first (finishValue).
    bool start(uint32_t now, uint32_t durationMs, int16_t &finishValue);

    // Called on every pass: true when a frame is due, with its progress (DISPLAY_ROLL_DONE: the last one, the roll is over)
    bool frame(uint32_t now, uint32_t durationMs, uint8_t &progress);

  private:
    bool requested_ = false;
    bool active_ = false;
    bool finishFirst_ = false;   // a new number came during the roll: the number of that roll is drawn as it is first
    int16_t finishValue_ = 0;    // that number
    uint32_t start_ = 0;
    uint32_t nextFrame_ = 0;
};
