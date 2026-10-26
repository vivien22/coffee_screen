/**
  This file is part of the CJMCU-111 rotary button demo distribution
  (https://github.com/ChrisPHL/CJMCU-111-rotary-button-demo).
  Copyright (c) 2020 Christian Pohl, aka ChrisPHL, www.chpohl.de.

  Packaged as a library from the demo code (ISR counting logic unchanged;
  press start time, push duration and short / double / long push events added).
  License: GNU General Public License v3 (see CJMCU111.h).
*/

#include "CJMCU111.h"

byte CJMCU111::rotaryPinGa = 2;
byte CJMCU111::rotaryPinGb = 3;

#ifdef CJMCU_111_DEBUG
byte CJMCU111::rotaryIsrDebugPinA = 4;
byte CJMCU111::rotaryIsrDebugPinB = 5;
#endif

volatile CJMCU111::RotationInputStatus CJMCU111::rotationStatus = CJMCU111::kRotationStatusWaiting;

volatile bool CJMCU111::rotaryInputDetected = false;
volatile byte CJMCU111::rotationCountL = 0;
volatile byte CJMCU111::rotationCountR = 0;
volatile byte CJMCU111::rotaryPushCount = 0;
volatile unsigned long CJMCU111::rotaryPushDuration = 0;
volatile unsigned long CJMCU111::rotaryPressStart = 0;
volatile bool CJMCU111::rotaryPressCandidate = false;

volatile unsigned long CJMCU111::isrGxTime = 0;
volatile unsigned long CJMCU111::isrGxLastTime = 0;


CJMCU111::CJMCU111(byte pinGa, byte pinGb, unsigned int longPushMs, unsigned int doublePushMs) {
  rotaryPinGa = pinGa;
  rotaryPinGb = pinGb;
  longPushThresholdMs = longPushMs;
  doublePushWindowMs = doublePushMs;
  shortPushPending = false;
  shortPushTime = 0;
  queuedPushEvent = PUSH_NONE;
  longPushReported = false;
}

void CJMCU111::begin() {
  rotationStatus = kRotationStatusWaiting;

  pinMode(rotaryPinGa, INPUT_PULLUP);
  pinMode(rotaryPinGb, INPUT_PULLUP);

#ifdef CJMCU_111_DEBUG
  pinMode(rotaryIsrDebugPinA, OUTPUT);
  digitalWrite(rotaryIsrDebugPinA, HIGH);
  pinMode(rotaryIsrDebugPinB, OUTPUT);
  digitalWrite(rotaryIsrDebugPinB, HIGH);
#endif

  attachInterrupt(digitalPinToInterrupt(rotaryPinGa), isrGx, CHANGE);
  attachInterrupt(digitalPinToInterrupt(rotaryPinGb), isrGx, CHANGE);
}

//===========

bool CJMCU111::pushHeld(unsigned long &heldMs) {
  noInterrupts(); // the status (enum) and the time are multi-byte values
  bool held = rotaryPressCandidate && (rotationStatus == kRotationStatusStartLorPush);
  unsigned long pressStart = rotaryPressStart;
  interrupts();
  heldMs = held ? (millis() - pressStart) : 0;
  return held;
}

bool CJMCU111::update(byte &left, byte &right, PushEvent &event) {
  left = 0;
  right = 0;
  event = PUSH_NONE;

  // an event queued by the previous call goes first (rotation is kept for the next call)
  if (queuedPushEvent != PUSH_NONE) {
    event = queuedPushEvent;
    queuedPushEvent = PUSH_NONE;
    return true;
  }

  // state of the button first, then the counters: a release between the two is not lost
  unsigned long heldMs = 0;
  bool held = pushHeld(heldMs);

  byte push = 0;
  unsigned long pushDurationMs = 0;
  read(left, right, push, pushDurationMs);

  if (longPushReported) {
    if (push) {
      // release of a long push already reported while held: ignore it
      longPushReported = false;
      push--;
    } else if (!held) {
      // the press ended without a push (rotation while pressed)
      longPushReported = false;
    }
  }

  if (push) {
    unsigned long now = millis();
    if (push >= 2) {
      // two pushes registered since the last call: double push
      shortPushPending = false;
      event = PUSH_DOUBLE;
    } else if (pushDurationMs >= longPushThresholdMs) {
      // long push not seen while held (update() was not called in time): report it on release
      if (shortPushPending) {
        shortPushPending = false;
        event = PUSH_SHORT;
        queuedPushEvent = PUSH_LONG;
      } else {
        event = PUSH_LONG;
      }
    } else if (shortPushPending && (now - shortPushTime) < doublePushWindowMs) {
      shortPushPending = false;
      event = PUSH_DOUBLE;
    } else {
      if (shortPushPending) event = PUSH_SHORT; // previous short push, not followed in time
      shortPushPending = true;
      shortPushTime = now;
    }
  } else if (held && !longPushReported && heldMs >= longPushThresholdMs) {
    // the button has been held long enough: report the long push now
    longPushReported = true;
    if (shortPushPending) {
      shortPushPending = false;
      event = PUSH_SHORT;
      queuedPushEvent = PUSH_LONG;
    } else {
      event = PUSH_LONG;
    }
  }

  // a short push alone: report it once the double push delay has passed
  if (event == PUSH_NONE && shortPushPending && (millis() - shortPushTime) >= doublePushWindowMs) {
    shortPushPending = false;
    event = PUSH_SHORT;
  }

  return left || right || event != PUSH_NONE;
}

// Equivalent of the "if (rotaryInputDetected) { ... }" block in the demo's loop()
bool CJMCU111::read(byte &left, byte &right, byte &push) {
  unsigned long pushDurationMs;
  return read(left, right, push, pushDurationMs);
}

bool CJMCU111::read(byte &left, byte &right, byte &push, unsigned long &pushDurationMs) {
  if (!rotaryInputDetected) return false;

  noInterrupts(); // necessary when reading a multi-byte variable from the ISR
  left = rotationCountL;
  right = rotationCountR;
  push = rotaryPushCount;
  pushDurationMs = rotaryPushDuration;
  rotationCountL = 0;
  rotationCountR = 0;
  rotaryPushCount = 0;
  rotaryPushDuration = 0;
  rotaryInputDetected = false;
  interrupts();
  return true;
}

void CJMCU111::isrGx() {
  /*
     This routine relies on not being interrupted by another (higher prioritzed) interrupt.
     This should be the case for the Arduino hardware.
  */
#ifdef CJMCU_111_DEBUG
  digitalWrite(rotaryIsrDebugPinA, LOW);
#endif

  byte rotationInputA = digitalRead(rotaryPinGa);
  byte rotationInputB = digitalRead(rotaryPinGb);

  isrGxTime = millis();

  switch (rotationStatus) {
    case kRotationStatusWaiting:
      if (HIGH == rotationInputA && LOW == rotationInputB) {
        rotationStatus = kRotationStatusStartR;
      }
      else if (LOW == rotationInputA && HIGH == rotationInputB) {
        rotationStatus = kRotationStatusStartLorPush;
        isrGxLastTime = isrGxTime; // debounce
        rotaryPressStart = isrGxTime;
        rotaryPressCandidate = true;
      }
      break;

    case kRotationStatusStartLorPush:
      if (HIGH == rotationInputA && HIGH == rotationInputB) {
        rotationStatus = kRotationStatusWaiting;
        rotaryPressCandidate = false;
        if ((isrGxTime - isrGxLastTime) > isrGxDebounceDuration) {
          ++rotaryPushCount;
          rotaryPushDuration = isrGxTime - isrGxLastTime; // press -> release, msec
          rotaryInputDetected = true;
        }
      }
      else if (LOW == rotationInputA && LOW == rotationInputB) {
        rotationStatus = kRotationStatusTurningL;
        rotaryPressCandidate = false;
      }
      break;

    case kRotationStatusStartR:
      if (HIGH == rotationInputA && HIGH == rotationInputB) {
        rotationStatus = kRotationStatusWaiting;
      }
      else if (LOW == rotationInputA && LOW == rotationInputB) {
        rotationStatus = kRotationStatusTurningR;
      }
      break;

    case kRotationStatusTurningL:
      if (LOW == rotationInputA && HIGH == rotationInputB) {
        rotationStatus = kRotationStatusStartLorPush;
      }
      else if (HIGH == rotationInputA && LOW == rotationInputB) {
        rotationStatus = kRotationStatusWaitForFinishL;
      }
      break;

    case kRotationStatusTurningR:
      if (HIGH == rotationInputA && LOW == rotationInputB) {
        rotationStatus = kRotationStatusStartR;
      }
      else if (LOW == rotationInputA && HIGH == rotationInputB) {
        rotationStatus = kRotationStatusWaitForFinishR;
      }
      break;

    case kRotationStatusWaitForFinishL:
      if (LOW == rotationInputA && LOW == rotationInputB) {
        rotationStatus = kRotationStatusTurningL;
      }
      else if (HIGH == rotationInputA && HIGH == rotationInputB) {
        rotationStatus = kRotationStatusWaiting;
        rotationCountR = 0;
        ++rotationCountL;
        rotaryInputDetected = true;
        isrGxLastTime = isrGxTime; // debounce
      }
      break;

    case kRotationStatusWaitForFinishR:
      if (LOW == rotationInputA && LOW == rotationInputB) {
        rotationStatus = kRotationStatusTurningR;
      }
      else if (HIGH == rotationInputA && HIGH == rotationInputB) {
        rotationStatus = kRotationStatusWaiting;
        rotationCountL = 0;
        ++rotationCountR;
        rotaryInputDetected = true;
        isrGxLastTime = isrGxTime;
      }
      break;
  }

#ifdef CJMCU_111_DEBUG
  digitalWrite(rotaryIsrDebugPinA, HIGH);
#endif
}
