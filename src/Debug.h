// Debug.h: serial traces of the sensor, removed from the build when SERIAL_DEBUG is not defined.
// (The tuning console of this branch uses the serial port too: keep it off unless you debug the sensor.)

#pragma once
#include <Arduino.h>

//#define SERIAL_DEBUG

#ifdef SERIAL_DEBUG
#define DEBUG_PRINT(...)   Serial.print(__VA_ARGS__)
#define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)   do { } while (0)
#define DEBUG_PRINTLN(...) do { } while (0)
#endif
