#pragma once
#include "Arduino.h"
#define MSBFIRST 1
#define SPI_MODE1 1
struct SPISettings { SPISettings(unsigned long,int,int){} };
// transfer() returns 0, or what the test decides with hook
struct SPIClass { uint8_t (*hook)(uint8_t) = nullptr;
  void begin(){} void beginTransaction(SPISettings){} void endTransaction(){} uint8_t transfer(uint8_t v){return hook ? hook(v) : 0;} };
extern SPIClass SPI;
