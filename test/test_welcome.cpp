// The welcome picture and the three icons, drawn by the real decoder of Display.cpp (compressed data of IconsData.h),
// are exactly the 2-bit source of tools/icons_raw.h. The welcome picture is looked at while it is shown (before
// endWelcome()), and the screen after its end.
#include <cstdio>
#include <vector>
#include "src/Config.h"
#include "src/display/Display.h"
#include "src/display/Layout.h"
// The source of the pictures, under other names (the compressed ones have the names ICON_...)
#define ICON_THERMOMETER RAW_THERMOMETER
#define ICON_DROP RAW_DROP
#define ICON_CLOCK RAW_CLOCK
#define ICON_WELCOME RAW_WELCOME
#include "../tools/icons_raw.h"
#include "TestSupport.h"

uint16_t frameBuffer[240 * 240];


// The color of a level: COLOR_ICON over COLOR_BG, 5/15, 10/15, 15/15 (the blend of Display.cpp)
static uint16_t colorOf(int level) {
  if (level == 0) return COLOR_BG;
  const int opacity = level == 3 ? 15 : (level == 2 ? 10 : 5);
  auto channel = [&](int shift, int mask) {
    const int bg = (COLOR_BG >> shift) & mask, icon = (COLOR_ICON >> shift) & mask;
    return bg + (icon - bg) * opacity / 15;
  };
  return (uint16_t)((channel(11, 0x1F) << 11) | (channel(5, 0x3F) << 5) | channel(0, 0x1F));
}

// Pixels of the picture at (x0, y0) that differ from the source
static int differences(const uint16_t *screen, const uint8_t *raw, int x0, int y0, int width, int height) {
  int different = 0;
  for (int i = 0; i < width * height; i++) {
    const int level = (raw[i >> 2] >> (6 - 2 * (i & 3))) & 3;
    if (screen[(y0 + i / width) * 240 + x0 + i % width] != colorOf(level)) different++;
  }
  return different;
}

// Pixels outside the rectangle that are not the background
static int outside(const uint16_t *screen, int x0, int y0, int width, int height) {
  int other = 0;
  for (int y = 0; y < 240; y++)
    for (int x = 0; x < 240; x++)
      if (!(x >= x0 && x < x0 + width && y >= y0 && y < y0 + height) && screen[y * 240 + x] != COLOR_BG) other++;
  return other;
}

int main() {
  beginDisplay();
  beginWelcome();

  // the welcome picture is on the screen and lit: the whole picture, nothing else
  CHECK(stubPins[PIN_TFT_BL] == HIGH);
  CHECK(differences(frameBuffer, RAW_WELCOME, WELCOME_X, WELCOME_Y, ICON_WELCOME_WIDTH, ICON_WELCOME_HEIGHT) == 0);
  CHECK(outside(frameBuffer, WELCOME_X, WELCOME_Y, ICON_WELCOME_WIDTH, ICON_WELCOME_HEIGHT) == 0);
  // and it is in the middle of the round screen
  CHECK(WELCOME_X == 72 && WELCOME_Y == 72);

  // its end: backlight off and the screen clear, ready for the real screen
  endWelcome();
  CHECK(stubPins[PIN_TFT_BL] == LOW);
  CHECK(outside(frameBuffer, 0, 0, 0, 0) == 0);

  // the three icons
  drawIcons();
  CHECK(differences(frameBuffer, RAW_THERMOMETER, ICON_THERMOMETER_X, ICON_THERMOMETER_Y, ICON_THERMOMETER_WIDTH, ICON_THERMOMETER_HEIGHT) == 0);
  CHECK(differences(frameBuffer, RAW_DROP, ICON_DROP_X, ICON_DROP_Y, ICON_DROP_WIDTH, ICON_DROP_HEIGHT) == 0);
  CHECK(differences(frameBuffer, RAW_CLOCK, ICON_CLOCK_X, ICON_CLOCK_Y, ICON_CLOCK_WIDTH, ICON_CLOCK_HEIGHT) == 0);

  printf(fails ? "%d FAIL\n" : "ALL OK\n", fails);
  return fails != 0;
}
