#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
typedef uint8_t byte;
#define PROGMEM
#define HIGH 1
#define LOW 0
#define INPUT_PULLUP 2
#define OUTPUT 1
#define CHANGE 1
#define HEX 16
#define A0 14
#define A1 15
#define A2 16
#define A5 19
class __FlashStringHelper;
#define F(x) ((const __FlashStringHelper *)(x))
#define constrain(a,lo,hi) ((a)<(lo)?(lo):((a)>(hi)?(hi):(a)))
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
unsigned long millis(); void delay(unsigned long);
void pinMode(uint8_t,uint8_t); void digitalWrite(uint8_t,uint8_t); int digitalRead(uint8_t);
void noInterrupts(); void interrupts();
// The serial port of the tests: what is printed is kept in out, what the "user types" is put in in
struct SerialT {
  std::string out, in;
  size_t inPos = 0;
  void print(const char *t) { out += t; }
  void print(const __FlashStringHelper *t) { out += (const char *)t; }
  void print(char c) { out += c; }
  void print(int v) { out += std::to_string(v); }
  void print(unsigned v) { out += std::to_string(v); }
  void print(long v) { out += std::to_string(v); }
  void print(unsigned long v) { out += std::to_string(v); }
  void print(double v, int digits = 2) { char b[40]; snprintf(b, sizeof b, "%.*f", digits, v); out += b; }
  template<class T> void println(T v) { print(v); out += '\n'; }
  template<class T> void println(T v, int digits) { print(v, digits); out += '\n'; }
  void begin(long) {}
  int available() { return (int)(in.size() - inPos); }
  int read() { return inPos < in.size() ? in[inPos++] : -1; }
};
extern SerialT Serial;
// The state of this stand-in (ArduinoStubs.cpp), driven by the tests
extern unsigned long stubNow;   // what millis() returns
extern int stubPins[32];        // the level written to each pin by digitalWrite(), read by digitalRead()
char* itoa(int,char*,int);
#define pgm_read_byte(p) (*(p))
#define pgm_read_word(p) (*(p))
#define attachInterrupt(a,b,c) 
#define digitalPinToInterrupt(x) x
