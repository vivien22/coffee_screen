// Debug.h: serial traces, removed from the build when SERIAL_DEBUG is not defined.

#pragma once
#include <Arduino.h>

//#define SERIAL_DEBUG

#ifdef SERIAL_DEBUG
#define DEBUG_BEGIN(baud)  Serial.begin(baud)
#define DEBUG_PRINT(...)   Serial.print(__VA_ARGS__)
#define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
#else
#define DEBUG_BEGIN(baud)  do { } while (0)
#define DEBUG_PRINT(...)   do { } while (0)
#define DEBUG_PRINTLN(...) do { } while (0)
#endif
