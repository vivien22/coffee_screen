/**
  This file is part of the CJMCU-111 rotary button demo distribution
  (https://github.com/ChrisPHL/CJMCU-111-rotary-button-demo).
  Copyright (c) 2020 Christian Pohl, aka ChrisPHL, www.chpohl.de.

  Packaged as a library from the demo code (ISR counting logic unchanged;
  press start time, push duration and short / double / long push events added).
  License: GNU General Public License v3 (see CJMCU111.h).
*/

#include "CJMCU111.h"

byte CJMCU111::rotaryPinGa_ = 2;
byte CJMCU111::rotaryPinGb_ = 3;

#ifdef CJMCU_111_DEBUG
byte CJMCU111::rotaryIsrDebugPinA_ = 4;
byte CJMCU111::rotaryIsrDebugPinB_ = 5;
#endif

volatile CJMCU111::RotationInputStatus CJMCU111::rotationStatus_ = CJMCU111::kRotationStatusWaiting;

volatile bool CJMCU111::rotaryInputDetected_ = false;
volatile byte CJMCU111::rotationCountL_ = 0;
volatile byte CJMCU111::rotationCountR_ = 0;
volatile byte CJMCU111::rotaryPushCount_ = 0;
volatile unsigned long CJMCU111::rotaryPushDuration_ = 0;
volatile unsigned long CJMCU111::rotaryPressStart_ = 0;
volatile unsigned long CJMCU111::rotaryPushPressTime_ = 0;
volatile bool CJMCU111::rotaryPressCandidate_ = false;

volatile unsigned long CJMCU111::isrGxTime_ = 0;
volatile unsigned long CJMCU111::isrGxLastTime_ = 0;


CJMCU111::CJMCU111(byte pinGa, byte pinGb, unsigned int longPushMs, unsigned int doublePushMs) {
  rotaryPinGa_ = pinGa;
  rotaryPinGb_ = pinGb;
  longPushThresholdMs_ = longPushMs;
  doublePushWindowMs_ = doublePushMs;
  shortPushPending_ = false;
  shortPushTime_ = 0;
  queuedPushEvent_ = PUSH_NONE;
  longPushReported_ = false;
  eventPressMs_ = 0;
  shortPushPressMs_ = 0;
  queuedEventPressMs_ = 0;
  lastReadPushPressMs_ = 0;
}

void CJMCU111::begin() {
  rotationStatus_ = kRotationStatusWaiting;

  pinMode(rotaryPinGa_, INPUT_PULLUP);
  pinMode(rotaryPinGb_, INPUT_PULLUP);

#ifdef CJMCU_111_DEBUG
  pinMode(rotaryIsrDebugPinA_, OUTPUT);
  digitalWrite(rotaryIsrDebugPinA_, HIGH);
  pinMode(rotaryIsrDebugPinB_, OUTPUT);
  digitalWrite(rotaryIsrDebugPinB_, HIGH);
#endif

  attachInterrupt(digitalPinToInterrupt(rotaryPinGa_), isrGx, CHANGE);
  attachInterrupt(digitalPinToInterrupt(rotaryPinGb_), isrGx, CHANGE);
}

//===========

bool CJMCU111::pushHeld(unsigned long &heldMs, unsigned long &pressStartMs) {
  noInterrupts(); // the status (enum) and the time are multi-byte values
  bool held = rotaryPressCandidate_ && (rotationStatus_ == kRotationStatusStartLorPush);
  pressStartMs = rotaryPressStart_;
  interrupts();
  heldMs = held ? (millis() - pressStartMs) : 0;
  return held;
}

unsigned long CJMCU111::eventPressTime() const {
  return eventPressMs_;
}

bool CJMCU111::update(byte &left, byte &right, PushEvent &event) {
  left = 0;
  right = 0;
  event = PUSH_NONE;

  // an event queued by the previous call goes first (rotation is kept for the next call)
  if (queuedPushEvent_ != PUSH_NONE) {
    event = queuedPushEvent_;
    eventPressMs_ = queuedEventPressMs_;
    queuedPushEvent_ = PUSH_NONE;
    return true;
  }

  // state of the button first, then the counters: a release between the two is not lost
  unsigned long heldMs = 0;
  unsigned long pressStartMs = 0;
  bool held = pushHeld(heldMs, pressStartMs);

  byte push = 0;
  unsigned long pushDurationMs = 0;
  read(left, right, push, pushDurationMs);

  if (longPushReported_) {
    if (push) {
      // release of a long push already reported while held: ignore it
      longPushReported_ = false;
      push--;
    } else if (!held) {
      // the press ended without a push (rotation while pressed)
      longPushReported_ = false;
    }
  }

  if (push) {
    unsigned long now = millis();
    if (push >= 2) {
      // two pushes registered since the last call: double push
      shortPushPending_ = false;
      event = PUSH_DOUBLE;
      eventPressMs_ = lastReadPushPressMs_;
    } else if (pushDurationMs >= longPushThresholdMs_) {
      // long push not seen while held (update() was not called in time): report it on release
      if (shortPushPending_) {
        shortPushPending_ = false;
        event = PUSH_SHORT;
        eventPressMs_ = shortPushPressMs_;
        queuedPushEvent_ = PUSH_LONG;
        queuedEventPressMs_ = lastReadPushPressMs_;
      } else {
        event = PUSH_LONG;
        eventPressMs_ = lastReadPushPressMs_;
      }
    } else if (shortPushPending_ && (now - shortPushTime_) < doublePushWindowMs_) {
      shortPushPending_ = false;
      event = PUSH_DOUBLE;
      eventPressMs_ = lastReadPushPressMs_;
    } else {
      if (shortPushPending_) {
        // previous short push, not followed in time
        event = PUSH_SHORT;
        eventPressMs_ = shortPushPressMs_;
      }
      shortPushPending_ = true;
      shortPushTime_ = now;
      shortPushPressMs_ = lastReadPushPressMs_;
    }
  } else if (held && !longPushReported_ && heldMs >= longPushThresholdMs_) {
    // the button has been held long enough: report the long push now
    longPushReported_ = true;
    if (shortPushPending_) {
      shortPushPending_ = false;
      event = PUSH_SHORT;
      eventPressMs_ = shortPushPressMs_;
      queuedPushEvent_ = PUSH_LONG;
      queuedEventPressMs_ = pressStartMs;
    } else {
      event = PUSH_LONG;
      eventPressMs_ = pressStartMs;
    }
  }

  // a short push alone: report it once the double push delay has passed
  if (event == PUSH_NONE && shortPushPending_ && (millis() - shortPushTime_) >= doublePushWindowMs_) {
    shortPushPending_ = false;
    event = PUSH_SHORT;
    eventPressMs_ = shortPushPressMs_;
  }

  return left || right || event != PUSH_NONE;
}

// Equivalent of the "if (rotaryInputDetected_) { ... }" block in the demo's loop()
bool CJMCU111::read(byte &left, byte &right, byte &push) {
  unsigned long pushDurationMs;
  return read(left, right, push, pushDurationMs);
}

bool CJMCU111::read(byte &left, byte &right, byte &push, unsigned long &pushDurationMs) {
  if (!rotaryInputDetected_) return false;

  noInterrupts(); // necessary when reading a multi-byte variable from the ISR
  left = rotationCountL_;
  right = rotationCountR_;
  push = rotaryPushCount_;
  pushDurationMs = rotaryPushDuration_;
  lastReadPushPressMs_ = rotaryPushPressTime_;
  rotationCountL_ = 0;
  rotationCountR_ = 0;
  rotaryPushCount_ = 0;
  rotaryPushDuration_ = 0;
  rotaryInputDetected_ = false;
  interrupts();
  return true;
}

void CJMCU111::isrGx() {
  /*
     This routine relies on not being interrupted by another (higher prioritzed) interrupt.
     This should be the case for the Arduino hardware.
  */
#ifdef CJMCU_111_DEBUG
  digitalWrite(rotaryIsrDebugPinA_, LOW);
#endif

  byte rotationInputA = digitalRead(rotaryPinGa_);
  byte rotationInputB = digitalRead(rotaryPinGb_);

  isrGxTime_ = millis();

  switch (rotationStatus_) {
    case kRotationStatusWaiting:
      if (HIGH == rotationInputA && LOW == rotationInputB) {
        rotationStatus_ = kRotationStatusStartR;
      }
      else if (LOW == rotationInputA && HIGH == rotationInputB) {
        rotationStatus_ = kRotationStatusStartLorPush;
        isrGxLastTime_ = isrGxTime_; // debounce
        rotaryPressStart_ = isrGxTime_;
        rotaryPressCandidate_ = true;
      }
      break;

    case kRotationStatusStartLorPush:
      if (HIGH == rotationInputA && HIGH == rotationInputB) {
        rotationStatus_ = kRotationStatusWaiting;
        // A push is Ga LOW then HIGH again while Gb never moved. If the knob started a left turn
        // (both LOW) and came back, the candidate was cancelled: it is not a push.
        const bool wasPress = rotaryPressCandidate_;
        rotaryPressCandidate_ = false;
        if (wasPress && (isrGxTime_ - isrGxLastTime_) > isrGxDebounceDuration_) {
          ++rotaryPushCount_;
          rotaryPushDuration_ = isrGxTime_ - isrGxLastTime_; // press -> release, msec
          rotaryPushPressTime_ = isrGxLastTime_;            // time of the press
          rotaryInputDetected_ = true;
        }
      }
      else if (LOW == rotationInputA && LOW == rotationInputB) {
        rotationStatus_ = kRotationStatusTurningL;
        rotaryPressCandidate_ = false;
      }
      break;

    case kRotationStatusStartR:
      if (HIGH == rotationInputA && HIGH == rotationInputB) {
        rotationStatus_ = kRotationStatusWaiting;
      }
      else if (LOW == rotationInputA && LOW == rotationInputB) {
        rotationStatus_ = kRotationStatusTurningR;
      }
      break;

    case kRotationStatusTurningL:
      if (LOW == rotationInputA && HIGH == rotationInputB) {
        rotationStatus_ = kRotationStatusStartLorPush;
      }
      else if (HIGH == rotationInputA && LOW == rotationInputB) {
        rotationStatus_ = kRotationStatusWaitForFinishL;
      }
      break;

    case kRotationStatusTurningR:
      if (HIGH == rotationInputA && LOW == rotationInputB) {
        rotationStatus_ = kRotationStatusStartR;
      }
      else if (LOW == rotationInputA && HIGH == rotationInputB) {
        rotationStatus_ = kRotationStatusWaitForFinishR;
      }
      break;

    case kRotationStatusWaitForFinishL:
      if (LOW == rotationInputA && LOW == rotationInputB) {
        rotationStatus_ = kRotationStatusTurningL;
      }
      else if (HIGH == rotationInputA && HIGH == rotationInputB) {
        rotationStatus_ = kRotationStatusWaiting;
        rotationCountR_ = 0;
        ++rotationCountL_;
        rotaryInputDetected_ = true;
        isrGxLastTime_ = isrGxTime_; // debounce
      }
      break;

    case kRotationStatusWaitForFinishR:
      if (LOW == rotationInputA && LOW == rotationInputB) {
        rotationStatus_ = kRotationStatusTurningR;
      }
      else if (HIGH == rotationInputA && HIGH == rotationInputB) {
        rotationStatus_ = kRotationStatusWaiting;
        rotationCountL_ = 0;
        ++rotationCountR_;
        rotaryInputDetected_ = true;
        isrGxLastTime_ = isrGxTime_;
      }
      break;
  }

#ifdef CJMCU_111_DEBUG
  digitalWrite(rotaryIsrDebugPinA_, HIGH);
#endif
}
