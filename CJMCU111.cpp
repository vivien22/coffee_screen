/**
  This file is part of the CJMCU-111 rotary button demo distribution
  (https://github.com/ChrisPHL/CJMCU-111-rotary-button-demo).
  Copyright (c) 2020 Christian Pohl, aka ChrisPHL, www.chpohl.de.

  Mise sous forme de bibliothèque du code de la démo (logique de l'ISR inchangée).
  Licence : GNU General Public License v3 (voir CJMCU111.h).
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

volatile unsigned long CJMCU111::isrGxTime = 0;
volatile unsigned long CJMCU111::isrGxLastTime = 0;


CJMCU111::CJMCU111(byte pinGa, byte pinGb) {
  rotaryPinGa = pinGa;
  rotaryPinGb = pinGb;
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

// Équivalent du bloc "if (rotaryInputDetected) { ... }" de loop() dans la démo
bool CJMCU111::read(byte &left, byte &right, byte &push) {
  if (!rotaryInputDetected) return false;

  noInterrupts(); // necessary when reading a multi-byte variable from the ISR
  left = rotationCountL;
  right = rotationCountR;
  push = rotaryPushCount;
  rotationCountL = 0;
  rotationCountR = 0;
  rotaryPushCount = 0;
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
      }
      break;

    case kRotationStatusStartLorPush:
      if (HIGH == rotationInputA && HIGH == rotationInputB) {
        rotationStatus = kRotationStatusWaiting;
        if ((isrGxTime - isrGxLastTime) > isrGxDebounceDuration) {
          ++rotaryPushCount;
          rotaryInputDetected = true;
        }
      }
      else if (LOW == rotationInputA && LOW == rotationInputB) {
        rotationStatus = kRotationStatusTurningL;
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
