/*
 * coffee_screen.ino, branch pid-tuning
 * Arduino Nano (ATmega328P, 32 KB Flash / 2 KB SRAM)
 *
 * Firmware to tune the boiler PID and its feedforward from the serial port (see README.md).
 * No screen, no encoder: everything is in TuningMachine (TuningMachine.h), which connects the modules:
 *
 *   Config.h               pins, limits, starting values of the tuning constants
 *   Debug.h                serial traces of the sensor (off)
 *   TemperatureSensor      MAX31865: reading, validity, filter
 *   HeaterController       PID + delayed feedforward, heater relay
 *   FlowSensor             flow sensors (USN_FlowSensor, PCINT1) or their simulator
 *
 * Safeties: the heater is switched off on MAX31865 fault or when the probe is above VALID_TEMPERATURE_MAX
 * (110 degrees C); the pump stops after the requested time and when no water flows; the watchdog resets
 * the board if the loop is blocked. None of this replaces an independent thermal fuse / thermostat.
 */

#include <avr/wdt.h>
#include "src/Config.h"
#include "src/machine/TuningMachine.h"

TuningMachine machine;

void setup()
{
#ifdef WATCHDOG_TIMEOUT
  // After a watchdog reset the watchdog is still running: stop it while the setup runs
  MCUSR = 0;
  wdt_disable();
#endif

  Serial.begin(SERIAL_BAUD);
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
