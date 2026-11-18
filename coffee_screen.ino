/*
 * coffee_screen.ino
 * Arduino Nano R3 (ATmega328P, 32 KB Flash / 2 KB SRAM) or Nano R4 (RA4M1, 256 KB / 32 KB): same code, the
 * differences (flow interrupts, watchdog) are in USN_FlowSensor.cpp and below
 *
 * Cooperative loop, no RTOS and no delay(). Everything is in CoffeeMachine
 * (CoffeeMachine.h), which connects the independent modules:
 *
 *   Config.h               pins, limits, timings, tuning constants
 *   Debug.h                serial traces
 *   Settings               saved values (EEPROM)
 *   TemperatureSensor      MAX31865: reading, validity, filter
 *   HeaterController       PID + feedforward, heater relay
 *   ExtractionController   extraction state machine, pump relay
 *   ScreenController       what is shown and when
 *   Display                drawing on the GC9A01 (Layout.h, Fonts.h)
 *   CJMCU111               rotary encoder + push button
 *   USN_FlowSensor         flow sensors (PCINT1 on the R3, external interrupts on the R4)
 *
 * Safeties: the heater is switched off on MAX31865 fault, implausible temperature or standby
 * (STANDBY_MINUTES without activity); the pump stops when no water flows (no-flow safety) and
 * the watchdog resets the board if the loop is blocked.
 * None of this replaces an independent thermal fuse / thermostat.
 */

#if defined(ARDUINO_ARCH_AVR)
#include <avr/wdt.h>
#else
#include <WDT.h>   // Nano R4 (Renesas core)
#endif
#include "src/Config.h"
#include "src/machine/CoffeeMachine.h"
#include "src/Debug.h"

CoffeeMachine machine;

void setup()
{
#if defined(WATCHDOG_TIMEOUT) && defined(ARDUINO_ARCH_AVR)
  // After a watchdog reset the watchdog is still running: stop it while the setup runs
  MCUSR = 0;
  wdt_disable();
#endif

  DEBUG_BEGIN(115200);
  machine.begin();

#ifdef WATCHDOG_TIMEOUT
#if defined(ARDUINO_ARCH_AVR)
  wdt_enable(WATCHDOG_TIMEOUT);
#else
  WDT.begin(WATCHDOG_MS);   // the watchdog of the R4 only starts here (it is not running during the setup)
#endif
#endif
}

void loop()
{
#ifdef WATCHDOG_TIMEOUT
#if defined(ARDUINO_ARCH_AVR)
  wdt_reset();   // if loop() is blocked longer than WATCHDOG_TIMEOUT, the board resets
#else
  WDT.refresh();
#endif
#endif
  machine.update();
}
