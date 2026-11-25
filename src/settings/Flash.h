/*
  Flash: writes into the EEPROM with as few block operations as possible.

  The EEPROM of the Arduino Nano R4 is an emulation on the 8 KB data flash, in blocks of 1 KB. Every call of the EEPROM library that
  changes something reads the whole block, changes the byte and ERASES and PROGRAMS the whole block again (some milliseconds, the
  loop of the machine waits). EEPROM.update() of ten bytes of the same block is ten block operations; EEPROM.put() groups the bytes of
  ONE object. flashWrite() groups the bytes of SEVERAL ranges: one erase + program per block that holds a change, and none when
  nothing changes. Each operation is counted and timed in the performance trace (drivers/Perf.h, command p of the console).

  Nothing here checks that the machine is not extracting: the callers keep the flash writes out of an active extraction (the stop of
  the pump must never wait for a block).
*/

#pragma once
#include <Arduino.h>

static const int FLASH_BLOCK_BYTES = 1024;   // the erase and program block of the data flash of the R4 (FLASH_BLOCK_SIZE of the core)

// The bytes to write at an address of the EEPROM
struct FlashRange {
  int address;
  const void *data;
  uint16_t size;
};

// Writes the ranges. Returns the number of block operations done (0 when every byte is already as wanted).
uint8_t flashWrite(const FlashRange *ranges, uint8_t count);

// One range
inline uint8_t flashWrite(int address, const void *data, uint16_t size) {
  const FlashRange range = {address, data, size};
  return flashWrite(&range, 1);
}
