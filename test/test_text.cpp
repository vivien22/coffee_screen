// The texts of the screen (units, phase tags, "ERR", the mode letter, the heater dot) drawn by putText() of Display.cpp: how many pixels each
// one sets, in which color, and where. The counts are those of the real Adafruit_GFX::print (measured with the library itself on the PC), so
// a text that is drawn differently fails here. The strings of F() are in the "flash" of the stub (scrambled, only pgm_read_byte() reads them
// back, as on the board): a code that reads such a string with *p draws garbage and fails too (that was the white artifacts on the units
// and on the dot of the heater).
#include <cstdio>
#include "src/Config.h"
#include "src/display/Display.h"
#include "src/display/Layout.h"
#include "TestSupport.h"

uint16_t frameBuffer[240 * 240];

// Pixels of a color in a rectangle of the screen
static int count(uint16_t color, int x0 = 0, int y0 = 0, int x1 = 240, int y1 = 240) {
  int n = 0;
  for (int y = y0; y < y1; y++)
    for (int x = x0; x < x1; x++)
      n += frameBuffer[y * 240 + x] == color;
  return n;
}

int main() {
  // the units, to the right of the number (the digits are in the cells before it)
  beginDisplay(); drawVolume(40, TEXT_SIZE_SMALL, PHASE_TAG_NONE, true);
  CHECK(count(COLOR_TEXT, VOLUME_X + 2 * DIGIT_CELL_SMALL, VOLUME_ZONE_Y, 240, VOLUME_ZONE_Y + VOLUME_ZONE_HEIGHT) == 521);   // "ml"
  beginDisplay(); drawDuration(5, TEXT_SIZE_SMALL, PHASE_TAG_NONE, true);
  CHECK(count(COLOR_TEXT, DURATION_X + DIGIT_CELL_SMALL, DURATION_ZONE_Y, 240, DURATION_ZONE_Y + DURATION_ZONE_HEIGHT) == 165);   // "s"
  beginDisplay(); drawTemperature(92.4, TEXT_SIZE_SMALL);
  // "92.4": three digits and the decimal point
  CHECK(count(COLOR_TEXT, TEMPERATURE_X + 3 * DIGIT_CELL_SMALL + 6, TEMPERATURE_ZONE_Y, 240, TEMPERATURE_ZONE_Y + TEMPERATURE_ZONE_HEIGHT) == 40 + 211);   // degree sign + "C"

  // the phase tags: orange, nothing else is orange on a screen without icons
  beginDisplay(); drawVolume(7, TEXT_SIZE_SMALL, PHASE_TAG_PREINFUSION, true);       CHECK(count(COLOR_TAG) == 345);    // PRE
  beginDisplay(); drawVolume(38, TEXT_SIZE_SMALL, PHASE_TAG_CALIBRATION, true);      CHECK(count(COLOR_TAG) == 279);    // CAL
  beginDisplay(); drawDuration(4, TEXT_SIZE_SMALL, PHASE_TAG_FLUSH, true);           CHECK(count(COLOR_TAG) == 525);    // FLUSH
  beginDisplay(); drawDuration(3, TEXT_SIZE_SMALL, PHASE_TAG_PREINFUSION_WAIT, true); CHECK(count(COLOR_TAG) == 438);   // WAIT

  // "ERR" of a probe fault: 30 px capitals (size 2) in the text color
  beginDisplay(); drawTemperatureError();
  CHECK(count(COLOR_TEXT) == 1476);

  // the heater dot: drawn, then erased completely
  beginDisplay(); drawHeaterIndicator(true);
  CHECK(count(COLOR_TEXT) == 37);
  CHECK(count(COLOR_TEXT, TEMPERATURE_DOT_X - 4, TEMPERATURE_DOT_Y - 4, TEMPERATURE_DOT_X + 4, TEMPERATURE_DOT_Y + 4) == 37);   // 7 x 7, around its cursor
  drawHeaterIndicator(false);
  CHECK(count(COLOR_BG) == 240 * 240);
  drawHeaterIndicator(true); drawHeaterIndicator(true); CHECK(count(COLOR_TEXT) == 37);          // drawn twice: the same
  drawHeaterIndicator(false); drawHeaterIndicator(false); CHECK(count(COLOR_BG) == 240 * 240);

  // Steaming mode: the temperature alone, large digits, in the zone in the middle of the screen
  clearScreen(); drawSteamTemperature(135.0, false);
  const int steam135 = count(COLOR_TEXT);
  CHECK(steam135 > 1500 && count(COLOR_TEXT, STEAM_ZONE_X, STEAM_ZONE_Y, STEAM_ZONE_X + STEAM_ZONE_WIDTH, STEAM_ZONE_Y + STEAM_ZONE_HEIGHT) == steam135);
  drawSteamTemperature(135.4, false); CHECK(count(COLOR_TEXT) == steam135);          // the same number again: the same picture
  drawSteamTemperature(96.0, false);                                                    // fewer digits: nothing is left of the old ones
  const int steam96 = count(COLOR_TEXT);
  clearScreen(); drawSteamTemperature(96.0, false); CHECK(count(COLOR_TEXT) == steam96 && steam96 < steam135);
  drawSteamTemperature(135.0, false); CHECK(count(COLOR_TEXT) == steam135);
  drawSteamTemperature(0.0, true); CHECK(count(COLOR_TEXT) > 1000 && count(COLOR_TEXT, 0, 0, 240, STEAM_ZONE_Y) == 0);   // ERR alone
  drawSteamTemperature(135.0, false); CHECK(count(COLOR_TEXT) == steam135);          // and the number again, ERR is gone
  clearScreen(); CHECK(count(COLOR_BG) == 240 * 240);
  // the text "STEAM" of the decoration (the real library draws the same letters: 5 letters, 66 px wide, centered under the number)
  clearScreen(); drawSteamIcon();
  printf("steam text px %d\n", count(COLOR_TAG, STEAM_TAG_X - 2, STEAM_TAG_BASELINE - 16, STEAM_TAG_X + 70, STEAM_TAG_BASELINE + 2));

#ifdef DISPLAY_ICONS
  // the letter in the drop: M has 31 pixels more than A
  beginDisplay(); drawDrop(true, false); const int withA = count(COLOR_TAG);
  beginDisplay(); drawDrop(true, true);  const int withM = count(COLOR_TAG);
  CHECK(withM - withA == 146 - 115);
#endif

  if (fails == 0) printf("test_text: ALL OK\n");
  return fails ? 1 : 0;
}
