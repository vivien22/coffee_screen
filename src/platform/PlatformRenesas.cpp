// PlatformRenesas.cpp: Platform.h for the Arduino Nano R4 (RA4M1, Renesas core). Empty on the Nano R3.
#include "../Config.h"

#if !defined(ARDUINO_ARCH_AVR)

#include "Platform.h"
#include <WDT.h>

// ---------- Watchdog ----------
// The watchdog of the R4 is not running during the setup: nothing to prepare, it starts at the end of it.
void platformWatchdogPrepare() {
}

void platformWatchdogStart() {
#ifdef WATCHDOG_TIMEOUT
  WDT.begin(WATCHDOG_MS);   // WATCHDOG_MS: at most about 5.5 s on the R4
#endif
}

void platformWatchdogKick() {
#ifdef WATCHDOG_TIMEOUT
  WDT.refresh();
#endif
}

// ---------- Pulses of the flow sensors ----------
// One external interrupt per input, on the falling edge (the inputs have a pull-up). The pins need an interrupt channel:
// A1 and A2 have one (6 and 7), A0 has none (see PIN_FLOW_IN in Config.h). The pin has to be set as an input BEFORE
// attachInterrupt() on this core.
#ifndef FLOW_SIMULATOR

void platformPulsesBegin() {
  pinMode(PIN_FLOW_IN, INPUT_PULLUP);
#ifdef FLOW_OUT_SENSOR
  pinMode(PIN_FLOW_OUT, INPUT_PULLUP);
#endif

  attachInterrupt(digitalPinToInterrupt(PIN_FLOW_IN), flowPulseIn, FALLING);
#ifdef FLOW_OUT_SENSOR
  attachInterrupt(digitalPinToInterrupt(PIN_FLOW_OUT), flowPulseOut, FALLING);
#endif
}

#endif  // FLOW_SIMULATOR

#endif  // !ARDUINO_ARCH_AVR
