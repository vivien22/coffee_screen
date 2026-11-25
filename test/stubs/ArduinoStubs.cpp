// The Arduino API of the host tests (and of tools/render_screens): a clock, the pins and the few library objects
// the code uses. The tests drive it through stubNow and stubPins (declared in Arduino.h, see TestSupport.h).
// Link it with every test program.
#include <stdio.h>
#include "Arduino.h"
#include "EEPROM.h"
#include "SPI.h"

unsigned long stubNow = 0;   // what millis() returns
int stubPins[32];            // the last level written to each pin; digitalRead() gives it back

unsigned long millis() { return stubNow; }
unsigned long micros() { return stubNow * 1000UL; }   // the clock of the tests has a resolution of 1 ms

// The "flash" of the stub: the strings of F() (see Arduino.h)
static uint8_t stubFlash[4096];
static size_t stubFlashUsed = 0;
static const char *stubFlashSources[128];
static const char *stubFlashCopies[128];
static int stubFlashCount = 0;

const char *stubFlashString(const char *literal) {
  for (int i = 0; i < stubFlashCount; i++) {
    if (stubFlashSources[i] == literal) return stubFlashCopies[i];
  }
  const size_t n = strlen(literal) + 1;
  if (stubFlashUsed + n > sizeof(stubFlash) || stubFlashCount >= 128) { fprintf(stderr, "stub flash full\n"); abort(); }
  for (size_t i = 0; i < n; i++) stubFlash[stubFlashUsed + i] = (uint8_t)literal[i] ^ 0x80;
  stubFlashSources[stubFlashCount] = literal;
  stubFlashCopies[stubFlashCount] = (const char *)&stubFlash[stubFlashUsed];
  stubFlashCount++;
  stubFlashUsed += n;
  return stubFlashCopies[stubFlashCount - 1];
}

uint8_t stubPgmReadByte(const void *address) {
  const uint8_t *p = (const uint8_t *)address;
  return (p >= stubFlash && p < stubFlash + sizeof(stubFlash)) ? (uint8_t)(*p ^ 0x80) : *p;
}
void (*stubDelayHook)(unsigned long) = nullptr;
void delay(unsigned long ms) { if (stubDelayHook) stubDelayHook(ms); }
void pinMode(uint8_t, uint8_t) {}
void digitalWrite(uint8_t pin, uint8_t level) { stubPins[pin] = level; }
int digitalRead(uint8_t pin) { return stubPins[pin]; }
void noInterrupts() {}
void interrupts() {}
char *itoa(int value, char *text, int) { sprintf(text, "%d", value); return text; }

SerialT Serial;
SPIClass SPI;
EEPROMClass EEPROM;
