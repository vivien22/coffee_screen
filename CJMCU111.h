/**
  This file is part of the CJMCU-111 rotary button demo distribution
  (https://github.com/ChrisPHL/CJMCU-111-rotary-button-demo).
  Copyright (c) 2020 Christian Pohl, aka ChrisPHL, www.chpohl.de.

  Mise sous forme de bibliothèque du code de la démo (logique de l'ISR inchangée).

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

// Décommenter pour sortir un signal sur deux broches pendant l'ISR (analyse à l'oscilloscope)
//#define CJMCU_111_DEBUG 1

class CJMCU111 {
  public:
    // pinGa et pinGb doivent être des broches d'interruption (D2 et D3 sur Nano / Uno)
    CJMCU111(byte pinGa = 2, byte pinGb = 3);

    // À appeler dans setup()
    void begin();

    // Renvoie true s'il y a eu une rotation ou un appui depuis le dernier appel.
    // left, right : crans tournés à gauche / à droite ; push : nombre d'appuis.
    // Les compteurs sont remis à zéro après lecture.
    bool read(byte &left, byte &right, byte &push);

  private:
    // Une seule instance possible : l'ISR et son état sont statiques
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

    static const byte isrGxDebounceDuration = 2; // msec

    static volatile unsigned long isrGxTime;
    static volatile unsigned long isrGxLastTime;
};

#endif
