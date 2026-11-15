#pragma once
#include "Arduino.h"
#define MSBFIRST 1
#define SPI_MODE1 1
struct SPISettings { SPISettings(unsigned long,int,int){} };
struct SPIClass { void begin(){} void beginTransaction(SPISettings){} void endTransaction(){} uint8_t transfer(uint8_t){return 0;} };
extern SPIClass SPI;
