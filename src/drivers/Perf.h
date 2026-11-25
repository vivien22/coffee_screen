/*
  Perf: a tiny trace of the time the machine spends in a few places, to find what blocks the loop (the screen animations and the
  heater need a pass every few milliseconds). Three categories are measured with micros():
    LOOP    one pass of CoffeeMachine::update() (without the serial console: printing the line is not a pass of the machine)
    SCREEN  the drawing, ScreenController::update()
    FLASH   one block operation of the data flash (erase + program of 1 KB, see settings/Flash.h)
  For each one the trace keeps the number of times and the longest time since the last reset. The serial console prints it with the
  command p (drivers/SerialConsole.h) and resets the maxima. Costs a few microseconds per pass.
*/

#pragma once
#include <Arduino.h>

enum PerfCategory : uint8_t { PERF_LOOP, PERF_SCREEN, PERF_FLASH, PERF_COUNT };

// One measure of that category, in microseconds
void perfRecord(PerfCategory category, uint32_t micros);

// The line for the console, without a newline:  PERF loop max 12.3 ms | screen max 9.8 ms | flash 3 ops max 45.1 ms
// then the counters are reset. Integers only (tenths of ms). Returns the length; never more than size bytes (size 100 is enough).
size_t perfTakeLine(char *out, size_t size);

// Measures the time of the block that contains it (the destructor records it)
class PerfScope {
  public:
    explicit PerfScope(PerfCategory category) : category_(category), start_(micros()) {}
    ~PerfScope() { perfRecord(category_, (uint32_t)(micros() - start_)); }
  private:
    PerfCategory category_;
    uint32_t start_;
};
