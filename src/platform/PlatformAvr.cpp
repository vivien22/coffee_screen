// PlatformAvr.cpp: Platform.h for the Arduino Nano R3 (ATmega328P). Empty on any other board.
#include "../Config.h"

#if defined(ARDUINO_ARCH_AVR)

#include "Platform.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/wdt.h>

#if !defined(__AVR_ATmega328P__)
#error "PlatformAvr.cpp requires an ATmega328P (classic Nano/Uno)."
#endif

// ---------- Watchdog ----------
void platformWatchdogPrepare() {
#ifdef WATCHDOG_TIMEOUT
  // After a watchdog reset the watchdog is still running: stop it while the setup runs
  MCUSR = 0;
  wdt_disable();
#endif
}

void platformWatchdogStart() {
#ifdef WATCHDOG_TIMEOUT
  wdt_enable(WATCHDOG_TIMEOUT);
#endif
}

void platformWatchdogKick() {
#ifdef WATCHDOG_TIMEOUT
  wdt_reset();   // if loop() is blocked longer than WATCHDOG_TIMEOUT, the board resets
#endif
}

// ---------- Pulses of the flow sensors ----------
// The two inputs are on port C (A0..A5): one pin change interrupt (PCINT1) serves both. The edges are found by comparing
// the state of the port with its state at the previous interrupt.
#ifndef FLOW_SIMULATOR

#define FLOW_IN_BIT   (PIN_FLOW_IN - A0)
#define FLOW_OUT_BIT  (PIN_FLOW_OUT - A0)

static volatile uint8_t lastPortC;

void platformPulsesBegin() {
  pinMode(PIN_FLOW_IN, INPUT_PULLUP);
#ifdef FLOW_OUT_SENSOR
  pinMode(PIN_FLOW_OUT, INPUT_PULLUP);
#endif

  // Synchronize the port state before enabling the interrupt.
  noInterrupts();
  lastPortC = PINC;
  PCMSK1 |= _BV(FLOW_IN_BIT);
#ifdef FLOW_OUT_SENSOR
  PCMSK1 |= _BV(FLOW_OUT_BIT);
#endif
  PCIFR |= _BV(PCIF1);  // Clear any pending PORTC pin-change flag.
  PCICR |= _BV(PCIE1);
  interrupts();
}

// PORTC pin-change interrupt (A0..A5): counts the falling edges (HIGH -> LOW), as used with INPUT_PULLUP.
ISR(PCINT1_vect) {
  const uint8_t currentPort = PINC;
  const uint8_t fallingEdges = lastPortC & (uint8_t)~currentPort;

  if (fallingEdges & _BV(FLOW_IN_BIT)) {
    flowPulseIn();
  }
#ifdef FLOW_OUT_SENSOR
  if (fallingEdges & _BV(FLOW_OUT_BIT)) {
    flowPulseOut();
  }
#endif

  lastPortC = currentPort;
}

#endif  // FLOW_SIMULATOR

#endif  // ARDUINO_ARCH_AVR
