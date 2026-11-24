// The texts of the screen (units, phase tags, "ERR", the mode letter, the heater dot) drawn by putText() of Display.cpp, in the
// anti-aliased fonts of Fonts.h (4 levels of opacity): where the ink is, in which colors, that nothing goes out of the round screen,
// that the heater dot is erased completely. The strings of F() are in the "flash" of the stub (scrambled, only pgm_read_byte() reads
// them back, as on the board): a code that reads such a string with *p draws garbage and fails too (that was the white artifacts on
// the units and on the dot of the heater).
#include <cstdio>
#include "src/Config.h"
#include "src/display/Display.h"
#include "src/display/Layout.h"
#include "TestSupport.h"

uint16_t frameBuffer[240 * 240];

// The rectangle of the pixels that are not the background, in a part of the screen (valid = false: there are none)
struct Ink {
  bool valid;
  int x0, y0, x1, y1;   // x1, y1: first column / row after the ink
  int count;
};

static Ink inkIn(int x0 = 0, int y0 = 0, int x1 = 240, int y1 = 240) {
  Ink ink = {false, 240, 240, 0, 0, 0};
  for (int y = y0; y < y1; y++) {
    for (int x = x0; x < x1; x++) {
      if (frameBuffer[y * 240 + x] == COLOR_BG) continue;
      ink.valid = true;
      ink.count++;
      if (x < ink.x0) ink.x0 = x;
      if (y < ink.y0) ink.y0 = y;
      if (x + 1 > ink.x1) ink.x1 = x + 1;
      if (y + 1 > ink.y1) ink.y1 = y + 1;
    }
  }
  return ink;
}

// Pixels of one color in the whole screen
static int count(uint16_t color) {
  int n = 0;
  for (int i = 0; i < 240 * 240; i++) n += frameBuffer[i] == color;
  return n;
}

// The 4 colors of a text of color fg: the background, a third, two thirds, the color (the formula of makePalette() in Display.cpp)
static uint16_t level(uint16_t fg, int level) {
  uint16_t color = 0;
  const int shifts[3] = {11, 5, 0}, masks[3] = {0x1F, 0x3F, 0x1F};
  for (int c = 0; c < 3; c++) {
    const int bg = (COLOR_BG >> shifts[c]) & masks[c], text = (fg >> shifts[c]) & masks[c];
    color |= (uint16_t)((bg + (text - bg) * level / 3) << shifts[c]);
  }
  return color;
}

// Every pixel of the screen is the background or one of the 3 other colors of the text (an anti-aliased glyph has nothing else)
static bool onlyColorsOf(uint16_t fg) {
  for (int i = 0; i < 240 * 240; i++) {
    const uint16_t p = frameBuffer[i];
    if (p != level(fg, 0) && p != level(fg, 1) && p != level(fg, 2) && p != level(fg, 3)) return false;
  }
  return true;
}

// Nothing is drawn outside the round screen (the radius of the display is 120 px; the corners are cut)
static bool insideTheRoundScreen() {
  for (int y = 0; y < 240; y++) {
    for (int x = 0; x < 240; x++) {
      const int dx = 2 * x + 1 - 240, dy = 2 * y + 1 - 240;   // twice the distance to the center, from the middle of the pixel
      if (frameBuffer[y * 240 + x] != COLOR_BG && dx * dx + dy * dy > 240 * 240) return false;
    }
  }
  return true;
}

int main() {
  // the units, to the right of the number (the digits are in the cells before it)
  beginDisplay(); drawVolume(40, TEXT_SIZE_SMALL, PHASE_TAG_NONE, true);
  const int afterVolume = VOLUME_X + 2 * DIGIT_CELL_SMALL;
  Ink ml = inkIn(afterVolume, VOLUME_ZONE_Y, 240, VOLUME_ZONE_Y + VOLUME_ZONE_HEIGHT);
  CHECK(ml.valid && ml.x0 >= afterVolume + VOLUME_UNIT_DX && ml.x1 - ml.x0 > 36 && ml.x1 - ml.x0 < 52);   // "ml"
  CHECK(ml.y1 == VOLUME_UNIT_BASELINE + 1 || ml.y1 == VOLUME_UNIT_BASELINE);                              // on its baseline
  CHECK(onlyColorsOf(COLOR_TEXT));

  beginDisplay(); drawDuration(5, TEXT_SIZE_SMALL, PHASE_TAG_NONE, true);
  Ink s = inkIn(DURATION_X + DIGIT_CELL_SMALL, DURATION_ZONE_Y, 240, DURATION_ZONE_Y + DURATION_ZONE_HEIGHT);
  CHECK(s.valid && s.x0 >= DURATION_X + DIGIT_CELL_SMALL + DURATION_UNIT_DX && s.x1 - s.x0 > 10 && s.x1 - s.x0 < 24);   // "s"

  beginDisplay(); drawTemperature(92.4, TEXT_SIZE_SMALL);
  // "92.4": three digits and the decimal point; then the degree sign and the "C"
  const int afterTemperature = TEMPERATURE_X + 3 * DIGIT_CELL_SMALL + 6;
  Ink degreeAndC = inkIn(afterTemperature, TEMPERATURE_ZONE_Y, 240, TEMPERATURE_ZONE_Y + TEMPERATURE_ZONE_HEIGHT);
  CHECK(degreeAndC.valid && degreeAndC.x1 - degreeAndC.x0 > 18 && degreeAndC.x1 - degreeAndC.x0 < 40);

  // the phase tags: orange, nothing else on a screen without icons; each one fits in the round screen, with some margin, and is not
  // wider than the old pixel font (PRE 36, CAL 36, WAIT 54, FLUSH 62) by more than 5 px
  struct { PhaseTag tag; bool inVolume; int maxWidth; } tags[4] = {
    {PHASE_TAG_PREINFUSION, true, 39}, {PHASE_TAG_CALIBRATION, true, 43}, {PHASE_TAG_PREINFUSION_WAIT, false, 57},
    {PHASE_TAG_FLUSH, false, 65}};
  for (int i = 0; i < 4; i++) {
    beginDisplay();
    if (tags[i].inVolume) drawVolume(7, TEXT_SIZE_SMALL, tags[i].tag, true); else drawDuration(4, TEXT_SIZE_SMALL, tags[i].tag, true);
    const int baseline = tags[i].inVolume ? VOLUME_TAG_BASELINE : DURATION_TAG_BASELINE;
    const int tagX = tags[i].inVolume ? VOLUME_TAG_X : DURATION_TAG_X;
    Ink tag = inkIn(tagX - 2, baseline - 16, 240, baseline + 1);
    CHECK(tag.valid && tag.x0 >= tagX && tag.x1 - tag.x0 <= tags[i].maxWidth && tag.y1 - tag.y0 >= 14);
    CHECK(count(COLOR_TAG) > 100);                        // the full color is there (4 levels, the heart of the strokes)
    CHECK(insideTheRoundScreen());
  }
  beginDisplay(); drawFlowLimit(25, true);                // "LIMIT" (48 px in the old font)
  Ink limit = inkIn(VOLUME_TAG_X - 2, VOLUME_TAG_BASELINE - 16, 240, VOLUME_TAG_BASELINE + 1);
  CHECK(limit.valid && limit.x1 - limit.x0 <= 54 && insideTheRoundScreen());

  // "ERR" of a probe fault: 30 px capitals, in the text color
  beginDisplay(); drawTemperatureError();
  Ink err = inkIn(TEMPERATURE_ZONE_X, 0, 240, TEMPERATURE_ZONE_Y + TEMPERATURE_ZONE_HEIGHT);
  CHECK(err.valid && err.x0 >= TEMPERATURE_X && err.x1 - err.x0 > 70 && err.x1 - err.x0 < 84 && err.y1 - err.y0 >= 29 && err.y1 - err.y0 <= 32);
  CHECK(err.y1 == TEMPERATURE_ERROR_BASELINE + 1 || err.y1 == TEMPERATURE_ERROR_BASELINE);
  CHECK(onlyColorsOf(COLOR_TEXT) && insideTheRoundScreen());

  // the heater dot: drawn, then erased completely
  beginDisplay(); drawHeaterIndicator(true);
  Ink dot = inkIn();
  CHECK(dot.valid && dot.x0 >= TEMPERATURE_DOT_X - 4 && dot.x1 <= TEMPERATURE_DOT_X + 5 && dot.y0 >= TEMPERATURE_DOT_Y - 4 && dot.y1 <= TEMPERATURE_DOT_Y + 5);
  CHECK(dot.x1 - dot.x0 >= 7 && dot.x1 - dot.x0 <= 8);                                  // a disc of 7 px
  CHECK(count(COLOR_TEXT) >= 15);                                                       // its heart has the full color
  const int dotInk = dot.count;
  drawHeaterIndicator(false);
  CHECK(count(COLOR_BG) == 240 * 240);
  drawHeaterIndicator(true); drawHeaterIndicator(true); CHECK(inkIn().count == dotInk);   // drawn twice: the same
  drawHeaterIndicator(false); drawHeaterIndicator(false); CHECK(count(COLOR_BG) == 240 * 240);

  // Steaming mode: the temperature alone, large digits, in the zone in the middle of the screen
  clearScreen(); drawSteamTemperature(135.0, false);
  const Ink steam135 = inkIn();
  CHECK(steam135.count > 1500 && inkIn(STEAM_ZONE_X, STEAM_ZONE_Y, STEAM_ZONE_X + STEAM_ZONE_WIDTH, STEAM_ZONE_Y + STEAM_ZONE_HEIGHT).count == steam135.count);
  drawSteamTemperature(135.4, false); CHECK(inkIn().count == steam135.count);           // the same number again: the same picture
  drawSteamTemperature(96.0, false);                                                    // fewer digits: nothing is left of the old ones
  const int steam96 = inkIn().count;
  clearScreen(); drawSteamTemperature(96.0, false); CHECK(inkIn().count == steam96 && steam96 < steam135.count);
  drawSteamTemperature(135.0, false); CHECK(inkIn().count == steam135.count);
  drawSteamTemperature(0.0, true);                                                      // ERR alone, centered
  const Ink steamErr = inkIn();
  CHECK(steamErr.count > 1000 && inkIn(0, 0, 240, STEAM_ZONE_Y).count == 0);
  CHECK(steamErr.x0 >= STEAM_ERROR_X && steamErr.x1 <= 240 - STEAM_ERROR_X + 1 && steamErr.y1 - steamErr.y0 >= 44 && steamErr.y1 - steamErr.y0 <= 47);
  drawSteamTemperature(135.0, false); CHECK(inkIn().count == steam135.count);           // and the number again, ERR is gone
  clearScreen(); CHECK(count(COLOR_BG) == 240 * 240);
  // the text "STEAM" of the decoration: 69 px wide at most, centered under the number
  clearScreen(); drawSteamIcon();
  Ink steamText = inkIn(0, STEAM_TAG_BASELINE - 16, 240, STEAM_TAG_BASELINE + 2);
  CHECK(steamText.valid && steamText.x1 - steamText.x0 <= 69 && steamText.x0 >= 240 - steamText.x1 - 2 && steamText.x0 <= 240 - steamText.x1 + 2);

#ifdef DISPLAY_ICONS
  // the letter in the drop: A and M differ, are in the same color as the tags, and are centered in the drop
  beginDisplay(); drawDrop(true, false); const Ink withA = inkIn(ICON_DROP_X, ICON_DROP_Y, ICON_DROP_X + ICON_DROP_WIDTH, ICON_DROP_Y + ICON_DROP_HEIGHT);
  const int pixelsA = count(COLOR_TAG);
  uint16_t pictureA[240 * 240]; for (int i = 0; i < 240 * 240; i++) pictureA[i] = frameBuffer[i];
  beginDisplay(); drawDrop(true, true);  const int pixelsM = count(COLOR_TAG);
  bool different = false; for (int i = 0; i < 240 * 240; i++) different |= pictureA[i] != frameBuffer[i];
  CHECK(different && pixelsM > pixelsA);                                                // M has more strokes than A
  CHECK(withA.valid && inkIn(ICON_DROP_X + ICON_DROP_WIDTH, 0, 240, 240).count == 0);   // nothing right of the drop
  // the letter is centered in the drop: the center of its ink, in the columns inside the ring of the icon, is the center of the drop
  for (int manual = 0; manual < 2; manual++) {
    beginDisplay(); drawDrop(true, manual);
    long weight = 0, sum = 0;
    for (int x = ICON_DROP_X + 8; x < ICON_DROP_X + ICON_DROP_WIDTH - 8; x++) {
      for (int y = ICON_DROP_Y + MODE_LETTER_DY - 16; y < ICON_DROP_Y + MODE_LETTER_DY + 3; y++) {
        if (frameBuffer[y * 240 + x] != COLOR_BG) { weight += x; sum++; }
      }
    }
    const double center = (double)weight / sum + 0.5;                    // in pixels from the left of the screen
    CHECK(center > ICON_DROP_X + ICON_DROP_WIDTH / 2.0 - 0.8 && center < ICON_DROP_X + ICON_DROP_WIDTH / 2.0 + 0.8);
  }
#endif

  if (fails == 0) printf("test_text: ALL OK\n");
  return fails ? 1 : 0;
}
