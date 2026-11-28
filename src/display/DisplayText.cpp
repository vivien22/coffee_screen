// DisplayText.cpp: the text engine of the display, see DisplayText.h.

#include <string.h>
#include "DisplayText.h"
#include "Layout.h"
#include "Screen.h"

// The brightness of the texts (see setTextBrightness): the color of the text is moved toward COLOR_BG
uint8_t textBrightness = 255;

void setTextBrightness(uint8_t level) {
  textBrightness = level;
}

static uint16_t dimChannel(uint8_t shift, uint8_t mask, uint16_t foreground) {
  const int16_t background = (COLOR_BG >> shift) & mask;
  const int16_t text = (foreground >> shift) & mask;
  return (uint16_t)(background + (text - background) * textBrightness / 255);
}

// A color moved toward COLOR_BG by the brightness (an icon at the brightness of a fade, see drawDropFrame)
uint16_t dimColor(uint16_t color) {
  return (dimChannel(11, 0x1F, color) << 11) | (dimChannel(5, 0x3F, color) << 5) | dimChannel(0, 0x1F, color);
}

static const uint8_t *glyphBitmap(const GFXfont *font) {
  return font->bitmap;
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
    tft.pushPixels(line, blockWidth);
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
void putText(const GFXfont *font, int16_t x, int16_t baseline, uint16_t color, Text text) {
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
    tft.pushPixels(line, width);
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
int16_t drawNumber(NumberField &field, const char *text, TextSize size, PhaseTag tag, bool force,
                   int16_t zoneX, int16_t zoneY, int16_t zoneWidth, int16_t zoneHeight,
                   int16_t x, int16_t baseline, bool &moved, bool threeLarge, uint8_t progress) {
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
    eraseZone(zoneX, zoneY, x - zoneX, zoneHeight);
  }
  if (moved && !repaint && xEnd < zoneX + zoneWidth) {
    eraseZone(xEnd, zoneY, zoneX + zoneWidth - xEnd, zoneHeight);
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
