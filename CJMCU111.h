/**
  This file is part of the CJMCU-111 rotary button demo distribution
  (https://github.com/ChrisPHL/CJMCU-111-rotary-button-demo).
  Copyright (c) 2020 Christian Pohl, aka ChrisPHL, www.chpohl.de.

  Packaged as a library from the demo code (ISR counting logic unchanged;
  press start time, push duration and short / double / long push events added).

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, version 3.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CJMCU111_H
#define CJMCU111_H

#include <Arduino.h>

// Uncomment to output a signal on two pins during the ISR (for oscilloscope analysis)
//#define CJMCU_111_DEBUG 1

// Push events returned by CJMCU111::update()
enum PushEvent {
  PUSH_NONE,     // nothing happened
  PUSH_SHORT,    // one short push (reported after the double push delay)
  PUSH_DOUBLE,   // two short pushes in a row
  PUSH_LONG      // push held for longPushMs (reported as soon as the delay has elapsed, button still held)
};

class CJMCU111 {
  public:
    // pinGa and pinGb must be interrupt pins (D2 and D3 on Nano / Uno)
    // longPushMs:   a push held this long (ms) is a long push, reported when the delay
    //               has elapsed (the release that follows is not reported again)
    // doublePushMs: max delay (ms) between two short pushes for a double push.
    //               A short push is reported once this delay has passed, 0 disables the double push.
    CJMCU111(byte pinGa = 2, byte pinGb = 3, unsigned int longPushMs = 800, unsigned int doublePushMs = 350);

    // Call in setup()
    void begin();

    // --- Recommended: rotation + classified push events ---
    // Call it on every loop(): the delay for a short push is checked here.
    // left, right: detents turned left / right since the last call.
    // event: PUSH_NONE, PUSH_SHORT, PUSH_DOUBLE or PUSH_LONG (one event per call).
    // Returns true if there was a rotation or a push event.
    bool update(byte &left, byte &right, PushEvent &event);

    // --- Raw reading (do not mix with update()) ---
    // Returns true if there was a rotation or a push since the last call.
    // left, right: detents turned left / right; push: number of pushes.
    // Counters are reset after reading.
    bool read(byte &left, byte &right, byte &push);

    // Same, and also returns the duration (ms) of the last push, measured from
    // press to release. Use it to tell a short push from a long push.
    bool read(byte &left, byte &right, byte &push, unsigned long &pushDurationMs);

  private:
    // Only one instance is possible: the ISR and its state are static
    static void isrGx();

    static byte rotaryPinGa;
    static byte rotaryPinGb;

#ifdef CJMCU_111_DEBUG
    static byte rotaryIsrDebugPinA;
    static byte rotaryIsrDebugPinB;
#endif

    enum RotationInputStatus {
      kRotationStatusWaiting, // Ga: HIGH and Gb: HIGH
      kRotationStatusStartLorPush, // Ga: FALLING, Gb: HIGH
      kRotationStatusStartR, // Ga: HIGH, Gb: FALLING
      kRotationStatusTurningL, // Ga: RISING, Gb: LOW
      kRotationStatusTurningR, // Ga: LOW, Gb: RISING
      kRotationStatusWaitForFinishL, // Ga: RISING, Gb: LOW
      kRotationStatusWaitForFinishR, // Ga: LOW, Gb RISING
    };

    static volatile RotationInputStatus rotationStatus;

    static volatile bool rotaryInputDetected;
    static volatile byte rotationCountL;
    static volatile byte rotationCountR;
    static volatile byte rotaryPushCount;
    static volatile unsigned long rotaryPushDuration; // msec, last push
    static volatile unsigned long rotaryPressStart;   // time (ms) the button was pressed
    static volatile bool rotaryPressCandidate;        // true while Ga stays LOW and Gb HIGH after a fresh press

    static const byte isrGxDebounceDuration = 2; // msec

    static volatile unsigned long isrGxTime;
    static volatile unsigned long isrGxLastTime;

    // Push classification (main context only, used by update())
    unsigned int longPushThresholdMs;
    unsigned int doublePushWindowMs;
    bool shortPushPending;            // a short push is waiting to know if a second one follows
    unsigned long shortPushTime;      // time (ms) of the pending short push
    PushEvent queuedPushEvent;        // event to return on the next update() call
    bool longPushReported;            // long push already reported while held: ignore its release

    // True while the button is held down; heldMs = time since the press
    bool pushHeld(unsigned long &heldMs);
};

#endif
