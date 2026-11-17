// What every test program shares: CHECK, the count of failures, and the clock and the pins of ArduinoStubs.cpp
// under the short names the tests use: t (what millis() returns, in ms) and pins[] (the level of each pin).
#pragma once
#include <stdio.h>
#include "Arduino.h"

static unsigned long &t = stubNow;
static int (&pins)[32] = stubPins;

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)
