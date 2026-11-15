/*
 * coffee_screen.ino
 * Arduino Nano (ATmega328P, 32 KB Flash / 2 KB SRAM)
 *
 * Cooperative loop, no RTOS and no delay(). Everything is in CoffeeMachine
 * (coffee_machine.h), which connects the independent modules:
 *
 *   config.h               pins, limits, timings, tuning constants
 *   debug.h                serial traces
 *   settings               saved values (EEPROM)
 *   temperature_sensor     MAX31865: reading, validity, filter
 *   heater_controller      PID + feedforward, heater relay
 *   extraction_controller  extraction state machine, pump relay
 *   screen_controller      what is shown and when
 *   display                drawing on the GC9A01 (icons.h, fonts.h)
 *   CJMCU111               rotary encoder + push button
 *   USN_FlowSensor         flow sensors (PCINT1)
 *
 * Heater control is disabled on MAX31865 fault or implausible temperature.
 * This is not a substitute for an independent thermal fuse/thermostat.
 */

#include <avr/wdt.h>
#include "src/config.h"
#include "src/machine/coffee_machine.h"
#include "src/debug.h"

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
