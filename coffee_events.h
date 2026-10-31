#pragma once
#include <Arduino.h>

// One-byte event mask. Events are generated and consumed in loop() context.
// This is intentionally not an ISR-safe general-purpose event queue.
enum CoffeeEvent : uint8_t {
  COFFEE_EVENT_NONE   = 0,
  COFFEE_EVENT_SHORT  = 1 << 0,
  COFFEE_EVENT_DOUBLE = 1 << 1,
  COFFEE_EVENT_LONG   = 1 << 2,
  COFFEE_EVENT_ENCODER= 1 << 3
};

void postCoffeeEvent(uint8_t eventMask);
uint8_t takeCoffeeEvents();

void addEncoderDelta(int16_t delta);
int16_t takeEncoderDelta();
