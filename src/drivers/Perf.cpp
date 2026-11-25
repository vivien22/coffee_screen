// Perf.cpp: see Perf.h

#include "Perf.h"
#include <stdio.h>

static uint32_t counts[PERF_COUNT];
static uint32_t maxima[PERF_COUNT];

void perfRecord(PerfCategory category, uint32_t micros) {
  counts[category]++;
  if (micros > maxima[category]) {
    maxima[category] = micros;
  }
}

// microseconds -> "12.3" (milliseconds, one decimal, rounded)
static void tenthsOfMs(char *text, size_t size, uint32_t micros) {
  const uint32_t tenths = (micros + 50) / 100;
  snprintf(text, size, "%lu.%lu", (unsigned long)(tenths / 10), (unsigned long)(tenths % 10));
}

size_t perfTakeLine(char *out, size_t size) {
  char loop[16], screen[16], flash[16];
  tenthsOfMs(loop, sizeof(loop), maxima[PERF_LOOP]);
  tenthsOfMs(screen, sizeof(screen), maxima[PERF_SCREEN]);
  tenthsOfMs(flash, sizeof(flash), maxima[PERF_FLASH]);
  const int n = snprintf(out, size, "PERF loop max %s ms | screen max %s ms | flash %lu ops max %s ms",
                         loop, screen, (unsigned long)counts[PERF_FLASH], flash);
  for (uint8_t i = 0; i < PERF_COUNT; i++) {
    counts[i] = 0;
    maxima[i] = 0;
  }
  if (n < 0 || size == 0) {
    return 0;
  }
  return ((size_t)n < size) ? (size_t)n : size - 1;
}
