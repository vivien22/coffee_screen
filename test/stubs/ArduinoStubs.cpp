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
