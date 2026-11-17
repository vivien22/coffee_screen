#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
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
#define F(x) (x)
#define constrain(a,lo,hi) ((a)<(lo)?(lo):((a)>(hi)?(hi):(a)))
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
unsigned long millis(); void delay(unsigned long);
void pinMode(uint8_t,uint8_t); void digitalWrite(uint8_t,uint8_t); int digitalRead(uint8_t);
void noInterrupts(); void interrupts();
struct SerialT { template<class T> void print(T){} template<class T,class U> void print(T,U){} template<class T> void println(T){} template<class T,class U> void println(T,U){} void begin(long){} };
extern SerialT Serial;
// The state of this stand-in (ArduinoStubs.cpp), driven by the tests
extern unsigned long stubNow;   // what millis() returns
extern int stubPins[32];        // the level written to each pin by digitalWrite(), read by digitalRead()
char* itoa(int,char*,int);
#define pgm_read_byte(p) (*(p))
#define pgm_read_word(p) (*(p))
#define attachInterrupt(a,b,c) 
#define digitalPinToInterrupt(x) x
