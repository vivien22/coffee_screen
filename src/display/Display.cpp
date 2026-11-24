/*
  display: drawing of the coffee screen (see Display.h).
*/

#include <string.h>
#include "Display.h"
#include "Fonts.h"
#include "Layout.h"

// ---------- Fonts (Poppins Bold, anti-aliased: 4 levels of opacity, see Fonts.h) ----------
#define FONT_SMALL (&FontSmall)   // digits, small size
#define FONT_LARGE (&FontLarge)   // digits, large size
#define FONT_UNIT  (&FontUnit)    // units: C, ml/s, degree sign and heater dot (codes '0'..'6', see Fonts.h)
#define FONT_TAG   (&FontTag)     // phase tag: PRE, WAIT, mode letter (15 px capitals)
#define FONT_ERROR_SMALL (&FontErrorSmall)   // "ERR" of a probe fault (30 px capitals)
#define FONT_ERROR_LARGE (&FontErrorLarge)   // "ERR" of a probe fault in the steaming mode (45 px capitals)

// The text of putText(): a string of flash on the board (F("...")), a plain pointer in the tests on the PC
#ifdef ARDUINO_STUB
typedef const char *Text;
#else
typedef const __FlashStringHelper *Text;
#endif

// The screen driver. Its text engine (Print::write -> Adafruit_GFX::drawChar, and the default 5 x 7 font) is not used: every text
// is drawn by putText() below. Overriding write() with nothing keeps that code, and the 1280 bytes of the default font, out of
// the firmware (a virtual function of the library is linked even if nobody calls it, unless a class replaces it).
class Screen : public Adafruit_GC9A01A {
  public:
    Screen(int8_t cs, int8_t dc, int8_t rst) : Adafruit_GC9A01A(cs, dc, rst) {}
    size_t write(uint8_t) override { return 0; }
};
static Screen tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

static void putText(const GFXfont *font, int16_t x, int16_t baseline, uint16_t color, Text text);
static const GFXglyph *glyphOf(const GFXfont *font, char c);

// What a number field shows, to erase only what is needed (see drawNumber)
struct NumberField {
  int16_t xEnd;     // where the number ended: the unit follows
  uint8_t size;     // TextSize of the digits
  uint8_t tag;      // PhaseTag shown next to it
  bool valid;       // false: the field is not known to show anything (start, or something else was drawn over it)
  bool repaint;     // the next drawing paints number, unit and tag over what is there, without erasing the zone first (repaintField)
  char text[6];     // the number of the last picture drawn as it is (the old number of a roll), see drawNumber
};
static NumberField temperatureField, volumeField, durationField;

// Starts the GC9A01 and clears the screen. The backlight stays off (until showDisplay()): nothing is seen while the
// screen is filled and drawn.
void beginDisplay() {
  // Set Blk Pin of the screen to Low in order to avoid artefacts before the screen is filled of black
  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, LOW);

  tft.begin();
  tft.fillScreen(COLOR_BG);
  temperatureField.valid = volumeField.valid = durationField.valid = false;
}

// Switches the backlight on: the screen appears as it has been drawn.
void showDisplay() {
  digitalWrite(PIN_TFT_BL, HIGH);
}

#ifdef DISPLAY_ICONS
// ------------------------------------------------------------
// Icons

// COLOR_ICON blended with COLOR_BG, opacity 0 (background) to 15 (full color), computed by the
// compiler: nothing is calculated on the board. The colors are RGB565 (5 bits red, 6 green, 5 blue);
// each channel is background + (icon - background) * opacity / 15.
#define BLEND_CHANNEL(shift, mask, opacity) \
  ((int)((COLOR_BG >> (shift)) & (mask)) + \
   ((int)((COLOR_ICON >> (shift)) & (mask)) - (int)((COLOR_BG >> (shift)) & (mask))) * (opacity) / 15)
#define BLEND_COLOR(opacity) \
  ((uint16_t)((BLEND_CHANNEL(11, 0x1F, opacity) << 11) | (BLEND_CHANNEL(5, 0x3F, opacity) << 5) | BLEND_CHANNEL(0, 0x1F, opacity)))

// The bits of a compressed icon (PROGMEM), most significant bit first
struct IconBits {
  const uint8_t *next;   // next byte to load
  uint8_t current;       // the bits of the byte not read yet, in the high bits
  uint8_t left;          // how many of them

  uint8_t get(uint8_t count) {
    uint8_t value = 0;
    while (count--) {
      if (!left) {
        current = pgm_read_byte(next++);
        left = 8;
      }
      value = (value << 1) | (current >> 7);
      current <<= 1;
      left--;
    }
    return value;
  }
};

// The brightness of the texts (see setTextBrightness): the color of the text is moved toward COLOR_BG
static uint8_t textBrightness = 255;

void setTextBrightness(uint8_t level) {
  textBrightness = level;
}

static uint16_t dimChannel(uint8_t shift, uint8_t mask, uint16_t foreground) {
  const int16_t background = (COLOR_BG >> shift) & mask;
  const int16_t text = (foreground >> shift) & mask;
  return (uint16_t)(background + (text - background) * textBrightness / 255);
}

// A color moved toward COLOR_BG by the brightness (an icon at the brightness of a fade, see drawDropFrame)
static uint16_t dimColor(uint16_t color) {
  return (dimChannel(11, 0x1F, color) << 11) | (dimChannel(5, 0x3F, color) << 5) | dimChannel(0, 0x1F, color);
}

// Draw an icon stored as 2-bit opacity, compressed (PROGMEM), row by row. The data is a stream of tokens
// (tools/compress_icons.cpp writes it from tools/icons_raw.h):
// the first two bits are the level of the pixels of the token:
//     00 nnnnn   nnnnn + 1 pixels of level 0       01   one pixel of level 1       10   one pixel of level 2
//     11 nnnn    nnnn + 1 pixels of level 3
//     00 11111 v mmmmmm   the escape (nnnnn = 31): mmmmmm + 1 pixels of level 1 (v = 0) or 2 (v = 1), for large areas
// Level 0 (background) is not drawn: the screen is already filled with COLOR_BG.
// Levels 1, 2, 3 are 5/15, 10/15, 15/15 of the icon color.
//
// A run is drawn as one burst of one color (a window of one row, then the pixels), not pixel by pixel: setting
// a window costs more than a pixel, so the runs of the icons and of the welcome picture are drawn several times
// faster. A run that reaches the end of a row goes on in the next one (a burst per row).
static void drawIcon(int16_t x, int16_t y, const uint8_t *data, uint8_t width, uint8_t height) {
  IconBits bits = {data, 0, 0};
  uint8_t row = 0, column = 0;   // the next pixel of the icon

  tft.startWrite();
  while (row < height) {
    uint8_t level = bits.get(2);
    uint8_t run = (level == 0 || level == 3) ? bits.get(level == 3 ? 4 : 5) + 1 : 1;
#if WELCOME_MS > 0
    if (level == 0 && run == 32) {   // the escape: a run of level 1 or 2 (only the welcome picture has such areas)
      level = 1 + bits.get(1);
      run = bits.get(6) + 1;
    }
#endif

    while (run) {
      const uint8_t part = (run < width - column) ? run : (uint8_t)(width - column);   // up to the end of the row
      if (level) {
        tft.setAddrWindow(x + column, y + row, part, 1);
        const uint16_t color = level == 3 ? BLEND_COLOR(15) : (level == 2 ? BLEND_COLOR(10) : BLEND_COLOR(5));
        tft.writeColor(textBrightness == 255 ? color : dimColor(color), part);
      }
      run -= part;
      column += part;
      if (column == width) {   // next row
        column = 0;
        row++;
      }
    }
  }
  tft.endWrite();
}

// The welcome picture, at start-up: drawn while the backlight is off, so that it appears complete, then the
// backlight goes on. It stays until endWelcome().
void beginWelcome() {
#if WELCOME_MS > 0
  drawIcon(WELCOME_X, WELCOME_Y, ICON_WELCOME, ICON_WELCOME_WIDTH, ICON_WELCOME_HEIGHT);
  digitalWrite(PIN_TFT_BL, HIGH);
#endif
}

// The end of the welcome picture: the backlight goes off and only the square of the picture is cleared (the rest of
// the screen is already clear: 6 times less to send than a whole screen, and nothing is seen anyway). The real screen
// is then drawn in the dark, and shown at once by showDisplay().
void endWelcome() {
#if WELCOME_MS > 0
  digitalWrite(PIN_TFT_BL, LOW);
  tft.fillRect(WELCOME_X, WELCOME_Y, ICON_WELCOME_WIDTH, ICON_WELCOME_HEIGHT, COLOR_BG);
#endif
}

// The standby: the welcome picture replaces the whole screen. The backlight goes off while the screen is cleared and
// the picture drawn (nothing is seen but the picture appearing complete), then it goes on. The values that were on the
// screen are gone: the number fields are not valid any more, so the real screen is drawn in full when it comes back
// (endWelcome() clears the picture, then the caller draws everything).
void showStandbyPicture() {
#if WELCOME_MS > 0
  digitalWrite(PIN_TFT_BL, LOW);
  clearScreen();
  beginWelcome();
#endif
}

// The fixed icons: thermometer, drop and clock.
void drawIcons() {
  drawIcon(ICON_THERMOMETER_X, ICON_THERMOMETER_Y, ICON_THERMOMETER, ICON_THERMOMETER_WIDTH, ICON_THERMOMETER_HEIGHT);
  drawIcon(ICON_DROP_X,        ICON_DROP_Y,        ICON_DROP,        ICON_DROP_WIDTH,        ICON_DROP_HEIGHT);
  drawIcon(ICON_CLOCK_X,       ICON_CLOCK_Y,       ICON_CLOCK,       ICON_CLOCK_WIDTH,       ICON_CLOCK_HEIGHT);
}

// What dresses the steaming screen (drawn once, never touched by the temperature): the thermometer left of the temperature, the
// volutes of steam above it and the text "STEAM" under it.
void drawSteamIcon() {
  drawIcon(STEAM_ICON_X, STEAM_ICON_Y, ICON_THERMOMETER, ICON_THERMOMETER_WIDTH, ICON_THERMOMETER_HEIGHT);
  drawIcon(STEAM_WISPS_X, STEAM_WISPS_Y, ICON_STEAM, ICON_STEAM_WIDTH, ICON_STEAM_HEIGHT);
  putText(FONT_TAG, STEAM_TAG_X, STEAM_TAG_BASELINE, COLOR_TAG, F(":624<"));   // FontTag codes: S T E A M
}

// The drop icon with the letter of the mode: A (AUTO) or M (MANUAL). Not visible = the icon is erased (blinking).
// The letter is centered in the drop by its own width (the letters of Poppins Bold have different widths), then moved by MODE_LETTER_DX.
void drawDrop(bool visible, bool manual) {
  tft.fillRect(ICON_DROP_X, ICON_DROP_Y, ICON_DROP_WIDTH, ICON_DROP_HEIGHT, COLOR_BG);
  if (visible) {
    drawIcon(ICON_DROP_X, ICON_DROP_Y, ICON_DROP, ICON_DROP_WIDTH, ICON_DROP_HEIGHT);
    drawModeLetter(manual);
  }
}

// The drop with its letter, drawn over what is there, at the brightness of setTextBrightness (a frame of the fade of a change of mode: the
// pixels of the icon replace the previous frame, nothing is erased first)
void drawDropFrame(bool manual) {
  drawIcon(ICON_DROP_X, ICON_DROP_Y, ICON_DROP, ICON_DROP_WIDTH, ICON_DROP_HEIGHT);
  drawModeLetter(manual);
}

// The letter of the mode alone, an opaque block (at the brightness of setTextBrightness): the frames of the fade of a change of mode
void drawModeLetter(bool manual) {
  const char letter = manual ? '<' : '4';   // FontTag codes: '4' = A, '<' = M
  const GFXglyph *glyph = glyphOf(FONT_TAG, letter);
  const int16_t letterWidth = pgm_read_byte(&glyph->width);
  const int16_t letterLeft = ICON_DROP_X + (ICON_DROP_WIDTH - letterWidth) / 2 + MODE_LETTER_DX;
  const char text[2] = {letter, 0};
  putText(FONT_TAG, letterLeft - (int8_t)pgm_read_byte(&glyph->xOffset), ICON_DROP_Y + MODE_LETTER_DY, COLOR_TAG,
          reinterpret_cast<Text>(text));
}
#else
void drawDrop(bool, bool) {}
void drawModeLetter(bool) {}
void drawDropFrame(bool) {}
void drawSteamIcon() {}
void beginWelcome() {}
void endWelcome() {}
void showStandbyPicture() {}
#endif  // DISPLAY_ICONS

// ------------------------------------------------------------
// Values

static const uint8_t *glyphBitmap(const GFXfont *font) {
  return font->bitmap;
}

// A glyph of a font. All the fonts start at '0'.
static const GFXglyph *glyphOf(const GFXfont *font, char c) {
  return &font->glyph[c - '0'];
}

// The fonts are masks of 2 bits per pixel (see Fonts.h): the level of a pixel, 0 (background) to 3 (full color).
static uint8_t levelAt(const uint8_t *bits, uint16_t pixel) {
  return (pgm_read_byte(&bits[pixel >> 2]) >> (6 - 2 * (pixel & 3))) & 3;
}

// The 4 colors of a text: COLOR_BG, then a third and two thirds of the way to the color of the text, then the color itself.
// Computed when a text is drawn (3 channels, 2 levels): nothing is stored. The colors are RGB565, 5 bits of red, 6 of green and
// 5 of blue: each channel is background + (text - background) * level / 3.
static uint16_t blendChannel(uint8_t shift, uint8_t mask, uint16_t foreground, uint8_t level) {
  const int16_t background = (COLOR_BG >> shift) & mask;
  const int16_t text = (foreground >> shift) & mask;
  return (uint16_t)(background + (text - background) * level / 3);
}

void forgetField(uint8_t field) {
  (field == 0 ? temperatureField : (field == 1 ? volumeField : durationField)).valid = false;
}

void repaintField(uint8_t field) {
  NumberField &f = (field == 0 ? temperatureField : (field == 1 ? volumeField : durationField));
  f.valid = false;
  f.repaint = true;
}

static void makePalette(uint16_t foreground, uint16_t palette[4]) {
  if (textBrightness != 255) {
    foreground = (dimChannel(11, 0x1F, foreground) << 11) | (dimChannel(5, 0x3F, foreground) << 5) | dimChannel(0, 0x1F, foreground);
  }
  for (uint8_t level = 0; level < 4; level++) {
    palette[level] = (blendChannel(11, 0x1F, foreground, level) << 11) | (blendChannel(5, 0x3F, foreground, level) << 5) |
                     blendChannel(0, 0x1F, foreground, level);
  }
}

// The cell of a digit has the same width for all the digits of a font (DIGIT_CELL_SMALL / DIGIT_CELL_LARGE in
// Layout.h), so that the end of a number, and the unit that follows it, move only when the number gets one digit
// longer or shorter, not each time a digit changes ("1" is much narrower than "4").

// Width of the cell of a character: how far the cursor moves after it.
static uint8_t advanceOf(const GFXfont *font, char c) {
  if (c == ':') {
    return pgm_read_byte(&glyphOf(font, c)->xAdvance);
  }
  return (font == FONT_SMALL) ? DIGIT_CELL_SMALL : DIGIT_CELL_LARGE;
}

// One glyph of the block of a digit: the character (0 = none) and how many rows it is moved down (negative: up) from its place.
struct CellGlyph {
  char code;
  int16_t shift;
};

// Draws a digit cell as a block of the zone height, in one burst: the glyph blended with the background (the 4 colors of the
// palette) and the background around it. The old character is replaced at once, without a moment where the place is blank (what a
// erase followed by a print would show). The block is as wide as the cell of the character, and the glyph is centered in it: no
// glyph goes beyond its cell (tools/generate_fonts.py cuts the 1 or 2 columns that would), so the blocks never overlap.
// The block is sent row by row: an anti-aliased glyph has no long runs of one color.
// Up to two glyphs share the cell, one leaving it and one coming in (the digits that roll, see drawNumber): on every row the most
// opaque of the two wins (they are never on the same rows: the shifts are a zone height apart).
static void drawCell(const GFXfont *font, const CellGlyph *glyphs, uint8_t count, int16_t x, int16_t y, int16_t height,
                     int16_t baseline, const uint16_t *palette) {
  const uint8_t blockWidth = advanceOf(font, glyphs[0].code);
  uint8_t levels[DIGIT_CELL_LARGE];   // one row of the block
  uint16_t line[DIGIT_CELL_LARGE];

  tft.startWrite();
  tft.setAddrWindow(x, y, blockWidth, height);
  for (int16_t row = 0; row < height; row++) {
    memset(levels, 0, blockWidth);
    for (uint8_t i = 0; i < count; i++) {
      const GFXglyph *glyph = glyphOf(font, glyphs[i].code);
      const uint8_t width = pgm_read_byte(&glyph->width);
      const int16_t glyphRow = y + row - (baseline + (int8_t)pgm_read_byte(&glyph->yOffset) + glyphs[i].shift);   // row of the glyph
      if (glyphRow < 0 || glyphRow >= pgm_read_byte(&glyph->height)) {
        continue;
      }
      // The glyph is centered in its (wider) cell
      const int16_t xOffset = (int8_t)pgm_read_byte(&glyph->xOffset)
                              + (blockWidth - pgm_read_byte(&glyph->xAdvance)) / 2;
      const uint8_t *bits = glyphBitmap(font) + pgm_read_word(&glyph->bitmapOffset);
      for (uint8_t column = 0; column < width; column++) {
        const int16_t position = xOffset + column;
        const uint8_t level = levelAt(bits, glyphRow * width + column);
        if (position >= 0 && position < blockWidth && level > levels[position]) {
          levels[position] = level;
        }
      }
    }
    for (uint8_t column = 0; column < blockWidth; column++) {
      line[column] = palette[levels[column]];
    }
    tft.writePixels(line, blockWidth);
  }
  tft.endWrite();
}

// The longest text of putText(): "ERR" in 45 px capitals (119 px)
#define TEXT_MAX_WIDTH 128

// A text with a font of the file, the cursor at x on the baseline: drawn as ONE block, the smallest rectangle around the glyphs, sent
// row by row (the pixels around the glyphs, inside the rectangle, are the background: the text is always drawn on it). The glyphs of a
// word can overlap by a pixel or two (their advance is a little shorter than their ink, see tools/generate_fonts.py): on every row
// the most opaque of the glyphs wins. The text is a string of FLASH on the board (F("...")): every character is read with
// pgm_read_byte(), never with *c (that would read the RAM at an address of the flash: garbage characters, garbage glyphs). The codes of
// the characters are those of Fonts.h. The text of the call has to be drawn on the background: the heater dot is erased by drawing it
// in the color of the background.
static void putText(const GFXfont *font, int16_t x, int16_t baseline, uint16_t color, Text text) {
  const char *characters = reinterpret_cast<const char *>(text);

  // The rectangle of the text: the union of the boxes of its glyphs
  int16_t left = 32767, right = -32768, top = 32767, bottom = -32768;
  int16_t cursor = x;
  for (const char *c = characters; pgm_read_byte(c); c++) {
    const GFXglyph *glyph = glyphOf(font, (char)pgm_read_byte(c));
    const int16_t glyphLeft = cursor + (int8_t)pgm_read_byte(&glyph->xOffset);
    const int16_t glyphTop = baseline + (int8_t)pgm_read_byte(&glyph->yOffset);
    left = min(left, glyphLeft);
    right = max(right, (int16_t)(glyphLeft + pgm_read_byte(&glyph->width)));
    top = min(top, glyphTop);
    bottom = max(bottom, (int16_t)(glyphTop + pgm_read_byte(&glyph->height)));
    cursor += pgm_read_byte(&glyph->xAdvance);
  }
  if (right <= left || right - left > TEXT_MAX_WIDTH) {
    return;   // nothing to draw (or a text longer than any text of the screen)
  }
  const int16_t width = right - left;

  uint16_t palette[4];
  makePalette(color, palette);
  uint8_t levels[TEXT_MAX_WIDTH];
  uint16_t line[TEXT_MAX_WIDTH];

  tft.startWrite();
  tft.setAddrWindow(left, top, width, bottom - top);
  for (int16_t row = top; row < bottom; row++) {
    memset(levels, 0, width);
    cursor = x;
    for (const char *c = characters; pgm_read_byte(c); c++) {
      const GFXglyph *glyph = glyphOf(font, (char)pgm_read_byte(c));
      const uint8_t glyphWidth = pgm_read_byte(&glyph->width);
      const int16_t glyphRow = row - (baseline + (int8_t)pgm_read_byte(&glyph->yOffset));
      if (glyphRow >= 0 && glyphRow < pgm_read_byte(&glyph->height)) {
        const uint8_t *bits = glyphBitmap(font) + pgm_read_word(&glyph->bitmapOffset);
        const int16_t first = cursor + (int8_t)pgm_read_byte(&glyph->xOffset) - left;   // column of the glyph in the rectangle
        for (uint8_t column = 0; column < glyphWidth; column++) {
          const uint8_t level = levelAt(bits, glyphRow * glyphWidth + column);
          if (level > levels[first + column]) {
            levels[first + column] = level;
          }
        }
      }
      cursor += pgm_read_byte(&glyph->xAdvance);
    }
    for (int16_t column = 0; column < width; column++) {
      line[column] = palette[levels[column]];
    }
    tft.writePixels(line, width);
  }
  tft.endWrite();
}

// Draws a number (small or large digits) left-aligned at x, on the given baseline. Every digit is a block that
// replaces what was there (see drawCell), so nothing is erased first and nothing flickers, even if the
// digits are the same as before. The rest of the zone is erased only where it is needed:
//   - left of the number and right of it, when the field did not show the same look before (first draw, other
//     digit size, other tag) or when force is set;
//   - right of the number, when it ends elsewhere than before (shorter number...).
// moved is set when the end of the number is not where it was: the unit that follows must be drawn again.
// Returns the x position after the number, where the unit goes.
//
// Rolling digits: progress is DISPLAY_ROLL_DONE (255) for a number drawn as it is. From 0 to 254 the picture is a frame of the roll
// from the number drawn last (field.text) to text: the digits that change leave upward while the new ones come up from below, shifted
// by progress / 255 of the height of the zone; the others do not move. At 0 the picture is the old number, and DISPLAY_ROLL_DONE draws
// the new one. A number that gains a digit (9 -> 10) rolls too: the unit moves to its new place on the first frame, like a number
// drawn as it is. There is no roll when the field has not been drawn in the same look (size, tag), or when the number loses digits
// (the leaving digit would be under the unit): the new number is then drawn as it is. field.text is the number of the last picture
// drawn as it is, so the frames of a roll always start from the same old number.
static int16_t drawNumber(NumberField &field, const char *text, TextSize size, PhaseTag tag, bool force,
                          int16_t zoneX, int16_t zoneY, int16_t zoneWidth, int16_t zoneHeight,
                          int16_t x, int16_t baseline, bool &moved, bool threeLarge = false, uint8_t progress = DISPLAY_ROLL_DONE) {
  // Three digits do not fit in large digits next to the other fields (the cells are wide): a long value is shown in small
  // digits (the steaming temperature has the whole screen)
  if (strlen(text) > 2 && !threeLarge) {
    size = TEXT_SIZE_SMALL;
  }

  const GFXfont *font = (size == TEXT_SIZE_SMALL) ? FONT_SMALL : FONT_LARGE;
  const bool sameLook = field.valid && !force && field.size == size && field.tag == tag;
  // A repainted field (the frames of a fade): the digits, the unit and the tag are opaque blocks that cover what they replace, so nothing
  // is erased first. An erase followed by a drawing would show the unit and the tag blinking at every frame.
  const bool repaint = field.repaint;
  field.repaint = false;
  const uint8_t length = strlen(text);
  const uint8_t oldLength = strlen(field.text);
  const bool rolling = progress < DISPLAY_ROLL_DONE && sameLook && oldLength > 0 && oldLength <= length;
  const int16_t shiftUp = rolling ? (int16_t)((uint32_t)zoneHeight * progress / DISPLAY_ROLL_DONE) : 0;

  // One block per digit, from left to right
  uint16_t palette[4];
  makePalette(COLOR_TEXT, palette);
  int16_t xEnd = x;
  for (uint8_t i = 0; i < length; i++) {
    CellGlyph glyphs[2];
    uint8_t count = 0;
    if (rolling && (length != oldLength || text[i] != field.text[i])) {
      glyphs[count++] = {text[i], (int16_t)(zoneHeight - shiftUp)};            // the new digit comes up from below
      if (i < oldLength) {
        glyphs[count++] = {field.text[i], (int16_t)(-shiftUp)};                // the old one leaves upward
      }
    } else {
      glyphs[count++] = {text[i], 0};
    }
    drawCell(font, glyphs, count, xEnd, zoneY, zoneHeight, baseline, palette);
    xEnd += advanceOf(font, text[i]);
  }

  moved = !sameLook || xEnd != field.xEnd;

  // Around the blocks: the left margin of the zone (new look), and the right of the number, up to the end of the
  // zone, if it moved (the old one was longer, the unit is going to be drawn again)
  if (!sameLook && !repaint && x > zoneX) {
    tft.fillRect(zoneX, zoneY, x - zoneX, zoneHeight, COLOR_BG);
  }
  if (moved && !repaint && xEnd < zoneX + zoneWidth) {
    tft.fillRect(xEnd, zoneY, zoneX + zoneWidth - xEnd, zoneHeight, COLOR_BG);
  }

  field.xEnd = xEnd;
  field.size = size;
  field.tag = tag;
  field.valid = true;
  if (progress >= DISPLAY_ROLL_DONE || !rolling) {   // a picture drawn as it is (a roll that is not possible is drawn as it is)
    strncpy(field.text, text, sizeof(field.text) - 1);
    field.text[sizeof(field.text) - 1] = 0;
  }
  return xEnd;
}

// Temperature: a decimal is added below 100 degrees (small digits only); the unit follows the number.
void drawTemperature(double value, TextSize size) {
  // Display one decimal below 100 °C; use an integer at 100 °C and above.
  const uint16_t roundedTenths = (uint16_t)(value * 10.0 + 0.5);
  const bool showDecimal = (value < 100 && roundedTenths < 1000);
  const uint16_t integerValue = showDecimal
    ? roundedTenths / 10
    : (uint16_t)(value + 0.5);

  // The decimal point is the glyph ':' of FontSmall (see Fonts.h): the number and its decimal are one text
  char text[6];
  itoa(integerValue, text, 10);
  if (showDecimal && size == TEXT_SIZE_SMALL) {
    const uint8_t length = strlen(text);
    text[length] = ':';
    text[length + 1] = (char)('0' + (roundedTenths % 10));
    text[length + 2] = 0;
  }

  bool moved;
  const int16_t xNumberEnd = drawNumber(temperatureField, text, size, PHASE_TAG_NONE, false,
                                        TEMPERATURE_ZONE_X, TEMPERATURE_ZONE_Y, TEMPERATURE_ZONE_WIDTH, TEMPERATURE_ZONE_HEIGHT,
                                        TEMPERATURE_X, TEMPERATURE_BASELINE, moved);

  if (moved) {
    // Degree sign (glyph '4' of FontUnit, centered on the cursor), then the unit
    putText(FONT_UNIT, xNumberEnd + TEMPERATURE_DEGREE_DX, TEMPERATURE_DEGREE_Y, COLOR_TEXT, F("4"));
    putText(FONT_UNIT, xNumberEnd + TEMPERATURE_UNIT_DX, TEMPERATURE_UNIT_BASELINE, COLOR_TEXT,
            F("0"));   // FontUnit codes: '0' = C, '2' '1' = ml, '3' = s (see Fonts.h)
  }
}

// The whole screen cleared (steaming mode, standby): the number fields are not valid any more.
void clearScreen() {
  tft.fillScreen(COLOR_BG);
  temperatureField.valid = volumeField.valid = durationField.valid = false;
}

// Steaming mode: the temperature in large digits (integer), left-aligned next to the thermometer; the unit follows it.
void drawSteamTemperature(double value, bool error) {
  if (error) {
    tft.fillRect(STEAM_ZONE_X, STEAM_ZONE_Y, STEAM_ZONE_WIDTH, STEAM_ZONE_HEIGHT, COLOR_BG);
    temperatureField.valid = false;
    putText(FONT_ERROR_LARGE, STEAM_ERROR_X, STEAM_ERROR_BASELINE, COLOR_TEXT, F("011"));   // FontError codes: E R R
    return;
  }
  char text[6];
  itoa(value < 0 ? 0 : (int)(value + 0.5), text, 10);
  bool moved;
  const int16_t xEnd = drawNumber(temperatureField, text, TEXT_SIZE_LARGE, PHASE_TAG_NONE, false,
                                  STEAM_ZONE_X, STEAM_ZONE_Y, STEAM_ZONE_WIDTH, STEAM_ZONE_HEIGHT,
                                  STEAM_X, STEAM_BASELINE, moved, true);
  if (moved) {
    putText(FONT_UNIT, xEnd + TEMPERATURE_DEGREE_DX, STEAM_DEGREE_Y, COLOR_TEXT, F("4"));
    putText(FONT_UNIT, xEnd + TEMPERATURE_UNIT_DX, STEAM_UNIT_BASELINE, COLOR_TEXT, F("0"));
  }
}

// "ERR" in white, in 30 px capitals: FontErrorSmall ('0' = E, '1' = R)
void drawTemperatureError() {
  tft.fillRect(TEMPERATURE_ZONE_X, TEMPERATURE_ZONE_Y, TEMPERATURE_ZONE_WIDTH, TEMPERATURE_ZONE_HEIGHT, COLOR_BG);
  temperatureField.valid = false;   // the temperature has to be drawn again in full
  putText(FONT_ERROR_SMALL, TEMPERATURE_X, TEMPERATURE_ERROR_BASELINE, COLOR_TEXT, F("011"));
}

// Phase tag at a given position. FontTag only holds the letters P R E W A I T F L U S H, under the
// codes '0'..';' (see Fonts.h): "012" is PRE, "3456" is WAIT and "789:;" is FLUSH.
static void drawTag(PhaseTag tag, int16_t x, int16_t baseline) {
  if (tag == PHASE_TAG_LIMIT) {
    putText(FONT_TAG, x, baseline, COLOR_TAG, F("85<56"));   // FontTag codes: L I M I T
    return;
  }
  // FontTag codes: P R E = "012", C A L = "=48", F L U S H = "789:;", W A I T = "3456"
  putText(FONT_TAG, x, baseline, COLOR_TAG,
          tag == PHASE_TAG_PREINFUSION ? F("012") :
          (tag == PHASE_TAG_CALIBRATION ? F("=48") : (tag == PHASE_TAG_FLUSH ? F("789:;") : F("3456"))));
}

// Volume in ml, with its tag (PRE, CAL...) when there is one. The tag is redrawn only if redrawTag.
void drawVolume(uint16_t value, TextSize size, PhaseTag tag, bool redrawTag, uint8_t progress) {
  // The tag already drawn is kept when only the number changes: the zone of the number stops where the
  // tag starts.
  const bool keepTag = (tag != PHASE_TAG_NONE) && !redrawTag;
  int16_t zoneWidth = VOLUME_ZONE_WIDTH;
  if (keepTag) {
    zoneWidth = VOLUME_TAG_X - VOLUME_ZONE_X;
  }

  char text[6];
  itoa(value, text, 10);

  bool moved;
  const int16_t xNumberEnd = drawNumber(volumeField, text, size, tag, tag != PHASE_TAG_NONE && redrawTag,
                                        VOLUME_ZONE_X, VOLUME_ZONE_Y, zoneWidth, VOLUME_ZONE_HEIGHT,
                                        VOLUME_X, VOLUME_BASELINE, moved, false, progress);

  if (moved) {
    // The "ml" (small or large digits) can reach under the tag, in a part of the field that the zone
    // above does not cover: erased too, below the tag.
    if (keepTag) {
      tft.fillRect(VOLUME_TAG_X, VOLUME_TAG_BASELINE + 1,
                   VOLUME_ZONE_X + VOLUME_ZONE_WIDTH - VOLUME_TAG_X,
                   VOLUME_ZONE_Y + VOLUME_ZONE_HEIGHT - (VOLUME_TAG_BASELINE + 1), COLOR_BG);
    }

    putText(FONT_UNIT, xNumberEnd + VOLUME_UNIT_DX, VOLUME_UNIT_BASELINE, COLOR_TEXT, F("21"));
  }

  if (tag != PHASE_TAG_NONE && redrawTag) {
    drawTag(tag, VOLUME_TAG_X, VOLUME_TAG_BASELINE);
  }
}

// The flow limit: tenths of ml/s, "2:5 ml/s" (the decimal point is the glyph ':' of FontSmall, as in drawTemperature), in the volume
// field with the tag LIMIT. The tag is drawn only if redrawTag; the number is drawn again only when it changed (drawNumber).
void drawFlowLimit(uint16_t tenths, bool redrawTag) {
  char text[6];
  itoa(tenths / 10, text, 10);
  const uint8_t length = strlen(text);
  text[length] = ':';
  text[length + 1] = (char)('0' + (tenths % 10));
  text[length + 2] = 0;

  const bool keepTag = !redrawTag;
  int16_t zoneWidth = VOLUME_ZONE_WIDTH;
  if (keepTag) {
    zoneWidth = VOLUME_TAG_X - VOLUME_ZONE_X;
  }
  bool moved;
  const int16_t xNumberEnd = drawNumber(volumeField, text, TEXT_SIZE_SMALL, PHASE_TAG_LIMIT, redrawTag,
                                        VOLUME_ZONE_X, VOLUME_ZONE_Y, zoneWidth, VOLUME_ZONE_HEIGHT,
                                        VOLUME_X, VOLUME_BASELINE, moved);
  if (moved) {
    if (keepTag) {
      tft.fillRect(VOLUME_TAG_X, VOLUME_TAG_BASELINE + 1,
                   VOLUME_ZONE_X + VOLUME_ZONE_WIDTH - VOLUME_TAG_X,
                   VOLUME_ZONE_Y + VOLUME_ZONE_HEIGHT - (VOLUME_TAG_BASELINE + 1), COLOR_BG);
    }
    putText(FONT_UNIT, xNumberEnd + VOLUME_UNIT_DX, VOLUME_UNIT_BASELINE, COLOR_TEXT, F("2163"));   // m l / s
  }
  if (redrawTag) {
    drawTag(PHASE_TAG_LIMIT, VOLUME_TAG_X, VOLUME_TAG_BASELINE);
  }
}

// Duration in seconds, with its tag (WAIT, FLUSH) when there is one.
void drawDuration(uint16_t value, TextSize size, PhaseTag tag, bool redrawTag, uint8_t progress) {
  // The tag already drawn is kept when only the number changes: the zone of the number stops where the
  // tag starts.
  const bool keepTag = (tag != PHASE_TAG_NONE) && !redrawTag;
  int16_t zoneWidth = DURATION_ZONE_WIDTH;
  if (keepTag) {
    zoneWidth = DURATION_TAG_X - DURATION_ZONE_X;
  }

  char text[6];
  itoa(value, text, 10);

  const bool wasLarge = durationField.valid && durationField.size == TEXT_SIZE_LARGE;

  bool moved;
  const int16_t xNumberEnd = drawNumber(durationField, text, size, tag, tag != PHASE_TAG_NONE && redrawTag,
                                        DURATION_ZONE_X, DURATION_ZONE_Y, zoneWidth, DURATION_ZONE_HEIGHT,
                                        DURATION_X, DURATION_BASELINE, moved, false, progress);

  if (moved) {
    // Large digits: the "s" is under the tag, in a part of the field that the zone above does not cover
    // (also when it was drawn there by the previous large value).
    if (keepTag && (size == TEXT_SIZE_LARGE || wasLarge)) {
      tft.fillRect(DURATION_TAG_X, DURATION_TAG_BASELINE + 1,
                   DURATION_ZONE_X + DURATION_ZONE_WIDTH - DURATION_TAG_X,
                   DURATION_ZONE_Y + DURATION_ZONE_HEIGHT - (DURATION_TAG_BASELINE + 1), COLOR_BG);
    }

    putText(FONT_UNIT, xNumberEnd + DURATION_UNIT_DX, DURATION_UNIT_BASELINE, COLOR_TEXT, F("3"));
  }

  if (tag != PHASE_TAG_NONE && redrawTag) {
    drawTag(tag, DURATION_TAG_X, DURATION_TAG_BASELINE);
  }
}

// Only the tag (used for the blinking of FLUSH).
void drawPhaseTag(PhaseTag tag, bool visible) {
  if (visible) {
    drawTag(tag, DURATION_TAG_X, DURATION_TAG_BASELINE);
  } else {
    // Only the rows of the letters: the number and its unit are not touched
    tft.fillRect(DURATION_TAG_X, DURATION_TAG_BASELINE - DURATION_TAG_HEIGHT,
                 DURATION_ZONE_X + DURATION_ZONE_WIDTH - DURATION_TAG_X, DURATION_TAG_HEIGHT, COLOR_BG);
  }
}

// The small dot next to the temperature that shows the heater is on.
void drawHeaterIndicator(bool visible) {
  // Glyph '5' of FontUnit, centered on the cursor: drawn in the background color it erases the dot
  putText(FONT_UNIT, TEMPERATURE_DOT_X, TEMPERATURE_DOT_Y, visible ? COLOR_TEXT : COLOR_BG, F("5"));
}
