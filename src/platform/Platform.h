// Platform.h: what differs between the boards (Nano R3 = ATmega328P, Nano R4 = RA4M1). The rest of the program is the same
// on both: GPIO, SPI, EEPROM, millis() come from the Arduino core of each board. Only this folder knows which board it is:
// PlatformAvr.cpp and PlatformRenesas.cpp each implement the functions below for one board (the file of the other board
// is empty, selected by ARDUINO_ARCH_AVR; plain functions, no virtual calls: nothing is paid in flash or RAM).
// Do not write board specific code (avr/..., PORTx, _BV, WDT...) outside this folder.

#pragma once
#include <Arduino.h>

// ---------- Watchdog (WATCHDOG_TIMEOUT in Config.h: not defined = no watchdog, these do nothing) ----------
// Call platformWatchdogPrepare() first thing in setup(), platformWatchdogStart() at the end of it (the welcome picture
// of several seconds is shown during the setup), and platformWatchdogKick() on every pass of loop().
void platformWatchdogPrepare();
void platformWatchdogStart();
void platformWatchdogKick();

// ---------- Pulses of the flow sensors ----------
// platformPulsesBegin() sets the input PIN_FLOW_IN (and PIN_FLOW_OUT with FLOW_OUT_SENSOR) as inputs with a pull-up and
// enables the interrupt that calls flowPulseIn() / flowPulseOut() on each falling edge. It is only used with the real
// flow sensors (not with FLOW_SIMULATOR). The two functions are defined by USN_FlowSensor.cpp and called from the
// interrupt: they must be short.
void platformPulsesBegin();
void flowPulseIn();
#ifdef FLOW_OUT_SENSOR
void flowPulseOut();
#endif
