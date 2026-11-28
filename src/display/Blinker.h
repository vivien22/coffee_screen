/*
  Blinker: something on the screen shown and hidden in turn, periodMs each, while it blinks (the FLUSH tag while flushing, the drop
  of the no-water error). It only keeps the time: the caller draws what visible() says when update() says it changed.

  hideAtOnce: the first change comes at the first update() after start() (the drop disappears at once); otherwise it comes periodMs
  later (the tag stays shown first).
*/

#pragma once
#include <stdint.h>

class Blinker {
  public:
    Blinker(uint32_t periodMs, bool hideAtOnce) : periodMs_(periodMs), hideAtOnce_(hideAtOnce) {}

    bool blinking() const { return blinking_; }
    bool visible() const { return visible_; }

    // Starts the blinking (nothing if it blinks already); the first change is taken by the next update()
    void start() {
      if (!blinking_) {
        blinking_ = true;
        pending_ = true;
      }
    }

    // Stops the blinking: it stays visible. Returns true if it was hidden (the caller draws it again).
    bool stop() {
      blinking_ = false;
      pending_ = false;
      const bool wasHidden = !visible_;
      visible_ = true;
      return wasHidden;
    }

    // Called on every pass while it may blink: true when visible() changed. hold: the change waits (it comes at a later call).
    bool update(uint32_t now, bool hold = false) {
      if (!blinking_) {
        return false;
      }
      if (pending_) {
        pending_ = false;
        toggleAt_ = now + periodMs_;
        if (hideAtOnce_) {
          visible_ = false;
          return true;
        }
        return false;
      }
      if ((int32_t)(now - toggleAt_) < 0 || hold) {
        return false;
      }
      visible_ = !visible_;
      toggleAt_ = now + periodMs_;
      return true;
    }

  private:
    uint32_t periodMs_;
    bool hideAtOnce_;
    bool blinking_ = false;
    bool pending_ = false;   // blinking just started: the first change time is set by update()
    bool visible_ = true;
    uint32_t toggleAt_ = 0;
};
