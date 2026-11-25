// The flash write helper (settings/Flash.cpp) and the block operations counted by the stub EEPROM: the data flash of the R4 is erased
// and programmed one block of 1 KB at a time, so what matters is the number of blocks that a write touches, not the number of bytes.
#include <cstdio>
#include <cstring>
#include "EEPROM.h"
#include "src/settings/Flash.h"
#include "TestSupport.h"

int main() {
  memset(EEPROM.mem, 0xFF, sizeof(EEPROM.mem));
  EEPROM.writes = EEPROM.blockOps = 0;

  // the stub counts like the board: update() that changes = 1 operation, nothing changed = 0, put() = 1 per block with a change
  EEPROM.update(10, 0x55); CHECK(EEPROM.blockOps == 1);
  EEPROM.update(10, 0x55); CHECK(EEPROM.blockOps == 1);
  struct Ten { uint8_t b[10]; } ten; memset(ten.b, 1, sizeof(ten.b));
  EEPROM.put(20, ten); CHECK(EEPROM.blockOps == 2 && EEPROM.writes == 11);
  EEPROM.put(20, ten); CHECK(EEPROM.blockOps == 2);
  EEPROM.put(1020, ten); CHECK(EEPROM.blockOps == 4);   // 4 bytes in the block 0, 6 in the block 1

  // the helper: one range
  EEPROM.blockOps = 0; EEPROM.writes = 0;
  uint8_t a[30]; for (int i = 0; i < 30; i++) a[i] = (uint8_t)(i + 1);
  CHECK(flashWrite(100, a, sizeof(a)) == 1 && EEPROM.blockOps == 1 && EEPROM.writes == 30 && memcmp(EEPROM.mem + 100, a, 30) == 0);
  CHECK(flashWrite(100, a, sizeof(a)) == 0 && EEPROM.blockOps == 1);                  // nothing changed: nothing written
  a[7] = 99;
  CHECK(flashWrite(100, a, sizeof(a)) == 1 && EEPROM.blockOps == 2 && EEPROM.writes == 31 && EEPROM.mem[107] == 99);   // one byte, one block operation
  CHECK(flashWrite(100, a, 0) == 0 && EEPROM.blockOps == 2);                          // no byte

  // several ranges in the same block: ONE operation (ten update() calls would be ten)
  uint8_t b = 5, c[4] = {1, 2, 3, 4}; uint16_t d = 0x1234;
  const FlashRange same[] = {{0, &b, 1}, {3, &d, 2}, {21, c, 4}};
  CHECK(flashWrite(same, 3) == 1 && EEPROM.blockOps == 3 && EEPROM.mem[0] == 5 && EEPROM.mem[3] == 0x34 && EEPROM.mem[4] == 0x12 && EEPROM.mem[24] == 4);
  CHECK(flashWrite(same, 3) == 0 && EEPROM.blockOps == 3);
  c[2] = 9;
  CHECK(flashWrite(same, 3) == 1 && EEPROM.blockOps == 4 && EEPROM.writes > 0 && EEPROM.mem[23] == 9);

  // ranges in two blocks: one operation for each; a block between them that nothing touches is left alone
  uint8_t e[4] = {7, 7, 7, 7};
  const FlashRange apart[] = {{10, e, 4}, {3000, e, 4}};
  EEPROM.blockOps = 0;
  CHECK(flashWrite(apart, 2) == 2 && EEPROM.blockOps == 2 && EEPROM.mem[3003] == 7);
  e[0] = 8;
  const FlashRange onlyFirst[] = {{10, e, 4}, {3000, e + 1, 3}};   // the second range is as it is
  EEPROM.blockOps = 0;
  CHECK(flashWrite(onlyFirst, 2) == 1 && EEPROM.blockOps == 1 && EEPROM.mem[10] == 8);

  // a range that crosses the border of a block: two operations if both sides change, one if only one side does
  uint8_t big[40]; memset(big, 0x42, sizeof(big));
  EEPROM.blockOps = 0;
  CHECK(flashWrite(1000, big, sizeof(big)) == 2 && EEPROM.blockOps == 2 && EEPROM.mem[1000] == 0x42 && EEPROM.mem[1039] == 0x42);
  big[39] = 0x43;
  CHECK(flashWrite(1000, big, sizeof(big)) == 1 && EEPROM.blockOps == 3 && EEPROM.mem[1039] == 0x43);
  big[0] = 0x44;
  CHECK(flashWrite(1000, big, sizeof(big)) == 1 && EEPROM.blockOps == 4 && EEPROM.mem[1000] == 0x44);

  // the last block of the data flash
  CHECK(flashWrite(8190, e, 2) == 1 && EEPROM.mem[8191] == e[1]);

  printf(fails ? "%d FAIL\n" : "ALL OK\n", fails); return fails != 0;
}
