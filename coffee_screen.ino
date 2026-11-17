/*
 * coffee_screen.ino
 * Arduino Nano (ATmega328P, 32 KB Flash / 2 KB SRAM)
 *
 * Cooperative loop, no RTOS and no delay(). Everything is in CoffeeMachine
 * (CoffeeMachine.h), which connects the independent modules:
 *
 *   Config.h               pins, limits, timings, tuning constants
 *   Debug.h                serial traces
 *   settings               saved values (EEPROM)
 *   temperature_sensor     MAX31865: reading, validity, filter
 *   heater_controller      PID + feedforward, heater relay
 *   extraction_controller  extraction state machine, pump relay
 *   screen_controller      what is shown and when
 *   display                drawing on the GC9A01 (Layout.h, Fonts.h)
 *   CJMCU111               rotary encoder + push button
 *   USN_FlowSensor         flow sensors (PCINT1)
 *
 * Safeties: the heater is switched off on MAX31865 fault, implausible temperature or standby
 * (STANDBY_MINUTES without activity); the pump stops when no water flows (no-flow safety) and
 * the watchdog resets the board if the loop is blocked.
 * None of this replaces an independent thermal fuse / thermostat.
 */

#include <avr/wdt.h>
#include "src/Config.h"
#include "src/machine/CoffeeMachine.h"
#include "src/Debug.h"

CoffeeMachine machine;

void setup()
{
#ifdef WATCHDOG_TIMEOUT
  // After a watchdog reset the watchdog is still running: stop it while the setup runs
  MCUSR = 0;
  wdt_disable();
#endif

  DEBUG_BEGIN(115200);
  machine.begin();

#ifdef WATCHDOG_TIMEOUT
  wdt_enable(WATCHDOG_TIMEOUT);
#endif
}

void loop()
{
#ifdef WATCHDOG_TIMEOUT
  wdt_reset();   // if loop() is blocked longer than WATCHDOG_TIMEOUT, the board resets
#endif
  machine.update();
}
