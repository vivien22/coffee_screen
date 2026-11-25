// Flash.cpp: see Flash.h

#include "Flash.h"
#include "../drivers/Perf.h"
#include <EEPROM.h>

// Is a byte of the block different from what is wanted?
static bool blockChanges(const FlashRange *ranges, uint8_t count, int block) {
  const int blockStart = block * FLASH_BLOCK_BYTES;
  const int blockEnd = blockStart + FLASH_BLOCK_BYTES;
  for (uint8_t r = 0; r < count; r++) {
    const uint8_t *data = (const uint8_t *)ranges[r].data;
    const int from = ranges[r].address > blockStart ? ranges[r].address : blockStart;
    const int to = ranges[r].address + ranges[r].size < blockEnd ? ranges[r].address + ranges[r].size : blockEnd;
    for (int address = from; address < to; address++) {
      if (EEPROM.read(address) != data[address - ranges[r].address]) {
        return true;
      }
    }
  }
  return false;
}

// One block: the bytes that changed are written, with one erase and one program. Returns false if the block could not be written.
static bool writeBlock(const FlashRange *ranges, uint8_t count, int block) {
  const int blockStart = block * FLASH_BLOCK_BYTES;
  const int blockEnd = blockStart + FLASH_BLOCK_BYTES;
  PerfScope scope(PERF_FLASH);

#ifdef ARDUINO_STUB
  EEPROM.blockOps++;   // the EEPROM of the tests counts the operations as the board does
#else
  veeprom &flash = veeprom::getInstance();   // what EEPROM.put() does, for the whole block at once
  uint32_t bytesToEnd = 0;
  if (flash.read_block(blockStart, bytesToEnd) != ReadStatus::ALLOCATED) {
    return false;   // no memory for the page: nothing written, the next call tries again
  }
#endif

  for (uint8_t r = 0; r < count; r++) {
    const uint8_t *data = (const uint8_t *)ranges[r].data;
    const int from = ranges[r].address > blockStart ? ranges[r].address : blockStart;
    const int to = ranges[r].address + ranges[r].size < blockEnd ? ranges[r].address + ranges[r].size : blockEnd;
    for (int address = from; address < to; address++) {
      const uint8_t value = data[address - ranges[r].address];
      if (EEPROM.read(address) == value) {
        continue;   // only the bytes that change are given to the flash: it erases the block as soon as one of them is not blank
      }
#ifdef ARDUINO_STUB
      EEPROM.mem[address] = value;
      EEPROM.writes++;
#else
      flash.write_byte(value, address);
#endif
    }
  }

#ifdef ARDUINO_STUB
  return true;
#else
  return flash.write();   // erase (if needed) and program of the block
#endif
}

uint8_t flashWrite(const FlashRange *ranges, uint8_t count) {
  int first = -1;
  int last = -1;
  for (uint8_t r = 0; r < count; r++) {
    if (ranges[r].size == 0) {
      continue;
    }
    const int start = ranges[r].address / FLASH_BLOCK_BYTES;
    const int end = (ranges[r].address + ranges[r].size - 1) / FLASH_BLOCK_BYTES;
    first = (first < 0 || start < first) ? start : first;
    last = (end > last) ? end : last;
  }

  uint8_t operations = 0;
  for (int block = first; first >= 0 && block <= last; block++) {
    if (blockChanges(ranges, count, block) && writeBlock(ranges, count, block)) {
      operations++;
    }
  }
  return operations;
}
