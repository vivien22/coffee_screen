#include "coffee_events.h"

static uint8_t pendingEvents = COFFEE_EVENT_NONE;
static int16_t pendingEncoderDelta = 0;

void postCoffeeEvent(uint8_t eventMask)
{
  pendingEvents |= eventMask;
}

uint8_t takeCoffeeEvents()
{
  const uint8_t result = pendingEvents;
  pendingEvents = COFFEE_EVENT_NONE;
  return result;
}

void addEncoderDelta(int16_t delta)
{
  // Saturate rather than overflow if a caller accumulates an abnormal amount.
  const int32_t sum = (int32_t)pendingEncoderDelta + delta;

  if (sum > 32767L) {
    pendingEncoderDelta = 32767;
  } else if (sum < -32768L) {
    pendingEncoderDelta = -32768;
  } else {
    pendingEncoderDelta = (int16_t)sum;
  }
}

int16_t takeEncoderDelta()
{
  const int16_t result = pendingEncoderDelta;
  pendingEncoderDelta = 0;
  return result;
}
