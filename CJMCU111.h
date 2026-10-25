/**
  This file is part of the CJMCU-111 rotary button demo distribution
  (https://github.com/ChrisPHL/CJMCU-111-rotary-button-demo).
  Copyright (c) 2020 Christian Pohl, aka ChrisPHL, www.chpohl.de.

  Packaged as a library from the demo code (ISR logic unchanged,
  push duration added).

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

class CJMCU111 {
  public:
    // pinGa and pinGb must be interrupt pins (D2 and D3 on Nano / Uno)
    CJMCU111(byte pinGa = 2, byte pinGb = 3);

    // Call in setup()
    void begin();

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

    static const byte isrGxDebounceDuration = 2; // msec

    static volatile unsigned long isrGxTime;
    static volatile unsigned long isrGxLastTime;
};

#endif
