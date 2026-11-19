/*
 * coffee_screen.ino
 * Arduino Nano R3 (ATmega328P, 32 KB Flash / 2 KB SRAM) or Nano R4 (RA4M1, 256 KB / 32 KB): same code, the
 * differences (flow interrupts, watchdog) are in src/platform/
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

#include "src/Config.h"
#include "src/platform/Platform.h"
#include "src/machine/CoffeeMachine.h"
#include "src/Debug.h"

CoffeeMachine machine;

void setup()
{
  platformWatchdogPrepare();   // after a watchdog reset the watchdog is still running (AVR): stopped during the setup

  DEBUG_BEGIN(115200);
  machine.begin();

  platformWatchdogStart();     // if WATCHDOG_TIMEOUT is defined: from here the loop has to run
}

void loop()
{
  platformWatchdogKick();      // if loop() is blocked longer than the watchdog timeout, the board resets
  machine.update();
}
