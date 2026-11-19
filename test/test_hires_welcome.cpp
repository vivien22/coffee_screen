// DISPLAY_HIRES (the Nano R4): the welcome picture, the icons and the digits drawn by the real Display.cpp are what
// HiResData.h holds. Built with -DDISPLAY_HIRES by run.sh (the other tests are the 1-bit look of the Nano R3).
#include <cstdio>
#include <cstdlib>
#include "src/Config.h"
#include "src/display/Display.h"
#include "src/display/Layout.h"
#include "TestSupport.h"

#ifndef DISPLAY_HIRES
#error "test_hires_welcome needs -DDISPLAY_HIRES"
#endif

uint16_t frameBuffer[240 * 240];

// COLOR_ICON over COLOR_BG by a coverage 0..255, within one unit of the 5 / 6 / 5 bits of each channel
static bool nearly(uint16_t a, uint16_t b) {
  const int shifts[3] = {11, 5, 0}, masks[3] = {31, 63, 31};
  for (int c = 0; c < 3; c++) {
    if (abs(((a >> shifts[c]) & masks[c]) - ((b >> shifts[c]) & masks[c])) > 1) return false;
  }
  return true;
}

static uint16_t expected(uint8_t coverage) {
  const int shifts[3] = {11, 5, 0}, masks[3] = {31, 63, 31};
  int color = 0;
  for (int c = 0; c < 3; c++) {
    const int bg = (COLOR_BG >> shifts[c]) & masks[c], icon = (COLOR_ICON >> shifts[c]) & masks[c];
    color |= (bg + ((icon - bg) * coverage + 127) / 255) << shifts[c];
  }
  return (uint16_t)color;
}

// Pixels of an icon at (x0, y0) that are not what its coverage gives (within one unit)
static int iconDifferences(const uint8_t *alpha, int x0, int y0, int width, int height) {
  int different = 0;
  for (int i = 0; i < width * height; i++) {
    if (!nearly(frameBuffer[(y0 + i / width) * 240 + x0 + i % width], expected(alpha[i]))) different++;
  }
  return different;
}

static int outside(int x0, int y0, int width, int height) {
  int other = 0;
  for (int y = 0; y < 240; y++)
    for (int x = 0; x < 240; x++)
      if (!(x >= x0 && x < x0 + width && y >= y0 && y < y0 + height) && frameBuffer[y * 240 + x] != COLOR_BG) other++;
  return other;
}

static int notBackground(int x0, int y0, int width, int height) {
  int count = 0;
  for (int y = y0; y < y0 + height; y++)
    for (int x = x0; x < x0 + width; x++)
      if (frameBuffer[y * 240 + x] != COLOR_BG) count++;
  return count;
}

int main() {
  beginDisplay();
  beginWelcome();

  // the welcome picture: exactly the RGB565 data, in the middle of the screen, nothing else; the backlight is on
  CHECK(stubPins[PIN_TFT_BL] == HIGH);
  int different = 0;
  for (int i = 0; i < HI_WELCOME_WIDTH * HI_WELCOME_HEIGHT; i++) {
    if (frameBuffer[(WELCOME_Y + i / HI_WELCOME_WIDTH) * 240 + WELCOME_X + i % HI_WELCOME_WIDTH] != HI_WELCOME[i]) different++;
  }
  CHECK(different == 0);
  CHECK(outside(WELCOME_X, WELCOME_Y, HI_WELCOME_WIDTH, HI_WELCOME_HEIGHT) == 0);
  CHECK(WELCOME_X * 2 + HI_WELCOME_WIDTH == 240 && WELCOME_Y * 2 + HI_WELCOME_HEIGHT == 240);
  // its corners are inside the round screen (radius 120)
  const int half = HI_WELCOME_WIDTH / 2;
  CHECK(half * half * 2 < 120 * 120);

  endWelcome();
  CHECK(stubPins[PIN_TFT_BL] == LOW);
  CHECK(outside(0, 0, 0, 0) == 0);

  // the three icons: the coverage of each pixel blended with the background
  drawIcons();
  CHECK(iconDifferences(HI_ICON_THERMOMETER, ICON_THERMOMETER_X, ICON_THERMOMETER_Y, HI_ICON_THERMOMETER_WIDTH, HI_ICON_THERMOMETER_HEIGHT) == 0);
  CHECK(iconDifferences(HI_ICON_DROP, ICON_DROP_X, ICON_DROP_Y, HI_ICON_DROP_WIDTH, HI_ICON_DROP_HEIGHT) == 0);
  CHECK(iconDifferences(HI_ICON_CLOCK, ICON_CLOCK_X, ICON_CLOCK_Y, HI_ICON_CLOCK_WIDTH, HI_ICON_CLOCK_HEIGHT) == 0);
  // an icon has full colour pixels (the outline) and soft ones (the edges)
  int full = 0, soft = 0;
  for (int i = 0; i < HI_ICON_CLOCK_WIDTH * HI_ICON_CLOCK_HEIGHT; i++) {
    full += HI_ICON_CLOCK[i] == 255;
    soft += HI_ICON_CLOCK[i] > 0 && HI_ICON_CLOCK[i] < 255;
  }
  CHECK(full > 50 && soft > 50);

  // the standby picture replaces the whole screen, then goes
  showStandbyPicture();
  CHECK(stubPins[PIN_TFT_BL] == HIGH);
  CHECK(outside(WELCOME_X, WELCOME_Y, HI_WELCOME_WIDTH, HI_WELCOME_HEIGHT) == 0);
  CHECK(notBackground(WELCOME_X, WELCOME_Y, HI_WELCOME_WIDTH, HI_WELCOME_HEIGHT) > 1000);
  endWelcome();
  CHECK(outside(0, 0, 0, 0) == 0);

  // the digits: no glyph goes beyond its cell (the blocks of two digits never overlap)
  for (int font = 0; font < 2; font++) {
    const HiFont &f = font == 0 ? HiFontSmall : HiFontLarge;
    const int cell = font == 0 ? DIGIT_CELL_SMALL : DIGIT_CELL_LARGE;
    for (char c = '0'; c <= '9'; c++) {
      const HiGlyph &g = f.glyphs[c - f.first];
      const int left = g.xOffset + (cell - g.advance) / 2;
      CHECK(left >= 0 && left + g.width <= cell);
    }
  }

  // a value is drawn inside its zone, in the text color, with soft edges
  beginDisplay();
  drawTemperature(92.4, TEXT_SIZE_SMALL);
  CHECK(notBackground(TEMPERATURE_ZONE_X, TEMPERATURE_ZONE_Y, TEMPERATURE_ZONE_WIDTH, TEMPERATURE_ZONE_HEIGHT) > 300);
  CHECK(outside(TEMPERATURE_ZONE_X, TEMPERATURE_ZONE_Y, TEMPERATURE_ZONE_WIDTH, TEMPERATURE_ZONE_HEIGHT) == 0);
  int text = 0, between = 0;
  for (int y = TEMPERATURE_ZONE_Y; y < TEMPERATURE_ZONE_Y + TEMPERATURE_ZONE_HEIGHT; y++)
    for (int x = TEMPERATURE_ZONE_X; x < TEMPERATURE_ZONE_X + TEMPERATURE_ZONE_WIDTH; x++) {
      const uint16_t p = frameBuffer[y * 240 + x];
      text += p == COLOR_TEXT;
      between += p != COLOR_TEXT && p != COLOR_BG;
    }
  CHECK(text > 100 && between > 40);                       // full colour inside the glyphs, intermediate colours on the edges

  // "ERR" of a probe fault: inside the zone of the temperature
  beginDisplay();
  drawTemperatureError();
  CHECK(notBackground(TEMPERATURE_ZONE_X, TEMPERATURE_ZONE_Y, TEMPERATURE_ZONE_WIDTH, TEMPERATURE_ZONE_HEIGHT) > 200);
  CHECK(outside(TEMPERATURE_ZONE_X, TEMPERATURE_ZONE_Y, TEMPERATURE_ZONE_WIDTH, TEMPERATURE_ZONE_HEIGHT) == 0);

  // the heater dot: drawn, then erased completely
  beginDisplay();
  drawHeaterIndicator(true);
  CHECK(notBackground(TEMPERATURE_DOT_X - 6, TEMPERATURE_DOT_Y - 6, 12, 12) > 10);
  drawHeaterIndicator(false);
  CHECK(outside(0, 0, 0, 0) == 0);

  if (fails == 0) printf("test_hires_welcome: ALL OK\n");
  return fails ? 1 : 0;
}
