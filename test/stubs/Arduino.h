#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
typedef uint8_t byte;
#define PROGMEM
#define ARDUINO_STUB 1   // the tests on the PC (Display.cpp: F() is a plain pointer here)
#define HIGH 1
#define LOW 0
#define INPUT_PULLUP 2
#define OUTPUT 1
#define CHANGE 1
#define HEX 16
#define A0 14
#define A1 15
#define A2 16
#define A3 17
#define A4 18
#define A5 19
// F("...") gives a string of the "flash" of the stub: its characters are scrambled (xor 0x80) and only pgm_read_byte() brings them back,
// as on the board where a plain read of a flash string gives the RAM at that address. A code that reads such a string with *p fails here.
const char *stubFlashString(const char *literal);
#define F(x) stubFlashString(x)
#define constrain(a,lo,hi) ((a)<(lo)?(lo):((a)>(hi)?(hi):(a)))
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
unsigned long millis(); unsigned long micros(); void delay(unsigned long);
void pinMode(uint8_t,uint8_t); void digitalWrite(uint8_t,uint8_t); int digitalRead(uint8_t);
void noInterrupts(); void interrupts();
struct SerialT {
  template<class T> void print(T){} template<class T,class U> void print(T,U){} template<class T> void println(T){} template<class T,class U> void println(T,U){} void begin(long){}
  // Text sent with print / println of a "const char *" is kept in out, and what a test puts in "in" is what read() gives (the serial console)
  std::string in, out;
  void print(const char *s) { out += s; }
  void println(const char *s) { out += s; out += "\n"; }
  int available() { return (int)in.size(); }
  int read() { if (in.empty()) return -1; const int c = (unsigned char)in[0]; in.erase(0, 1); return c; }
};
extern SerialT Serial;
// The state of this stand-in (ArduinoStubs.cpp), driven by the tests
extern unsigned long stubNow;   // what millis() returns
extern int stubPins[32];        // the level written to each pin by digitalWrite(), read by digitalRead()
extern void (*stubDelayHook)(unsigned long);   // called by delay(), if set (a test can look at the screen meanwhile)
char* itoa(int,char*,int);
uint8_t stubPgmReadByte(const void *address);
#define pgm_read_byte(p) stubPgmReadByte(p)
#define pgm_read_word(p) (*(p))
#define attachInterrupt(a,b,c) 
#define digitalPinToInterrupt(x) x
