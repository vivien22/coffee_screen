// Platform.h: what is specific to the board (Nano R4 = RA4M1, Renesas core): the watchdog and the interrupts of the flow
// sensors. GPIO, SPI, EEPROM and millis() come from the Arduino core. PlatformRenesas.cpp implements the functions below.
// (The Nano R3 had its own PlatformAvr.cpp: it is on the branch r3.) Keep the board specific code (WDT, attachInterrupt...)
// in this folder.

#pragma once
#include <Arduino.h>

// ---------- Watchdog (WATCHDOG_MS in Config.h: not defined = no watchdog, these do nothing) ----------
// Call platformWatchdogPrepare() first thing in setup(), platformWatchdogStart() at the end of it (the welcome picture
// of several seconds is shown during the setup), and platformWatchdogKick() on every pass of loop().
void platformWatchdogPrepare();
void platformWatchdogStart();
void platformWatchdogKick();

// ---------- Pulses of the flow sensors ----------
// platformPulsesBegin() sets the inputs PIN_FLOW_IN and PIN_FLOW_OUT (only with FLOW_OUT_SENSOR) as inputs with a pull-up and enables the interrupt that
// calls flowPulseIn() / flowPulseOut() on each falling edge. The two functions are defined by FlowSensor.cpp and called
// from the interrupt: they must be short.
void platformPulsesBegin();
void flowPulseIn();
void flowPulseOut();
