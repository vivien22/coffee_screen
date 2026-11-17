/*
  display: drawing of the coffee screen (see Display.h).
*/

#include <string.h>
#include "Display.h"
#include "Fonts.h"
#include "Layout.h"

// ---------- Fonts (digits in two sizes, units) ----------
#define FONT_SMALL (&FontSmall)   // digits, small size
#define FONT_LARGE (&FontLarge)   // digits, large size
#define FONT_UNIT  (&FontUnit)    // units: C, ml, s, degree sign and heater dot (codes '0'..'5', see Fonts.h)
#define FONT_TAG   (&FontTag)     // phase tag: PRE, WAIT (15 px capitals)

// The screen driver
static Adafruit_GC9A01A tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

// What a number field shows, to erase only what is needed (see drawNumber)
struct NumberField {
  int16_t xEnd;     // where the number ended: the unit follows
  uint8_t size;     // TextSize of the digits
  uint8_t tag;      // PhaseTag shown next to it
  bool valid;       // false: the field is not known to show anything (start, or something else was drawn over it)
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
    if (level == 0 && run == 32) {   // the escape: a run of level 1 or 2
      level = 1 + bits.get(1);
      run = bits.get(6) + 1;
    }

    while (run && row < height) {
      const uint8_t part = (run < width - column) ? run : (uint8_t)(width - column);   // up to the end of the row
      if (level) {
        tft.setAddrWindow(x + column, y + row, part, 1);
        tft.writeColor(level == 3 ? BLEND_COLOR(15) : (level == 2 ? BLEND_COLOR(10) : BLEND_COLOR(5)), part);
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

// The fixed icons: thermometer, drop and clock.
void drawIcons() {
  drawIcon(ICON_THERMOMETER_X, ICON_THERMOMETER_Y, ICON_THERMOMETER, ICON_THERMOMETER_WIDTH, ICON_THERMOMETER_HEIGHT);
  drawIcon(ICON_DROP_X,        ICON_DROP_Y,        ICON_DROP,        ICON_DROP_WIDTH,        ICON_DROP_HEIGHT);
  drawIcon(ICON_CLOCK_X,       ICON_CLOCK_Y,       ICON_CLOCK,       ICON_CLOCK_WIDTH,       ICON_CLOCK_HEIGHT);
}

// The drop icon with the letter of the mode: A (AUTO) or M (MANUAL). Not visible = the icon is erased (blinking).
void drawDrop(bool visible, bool manual) {
  tft.fillRect(ICON_DROP_X, ICON_DROP_Y, ICON_DROP_WIDTH, ICON_DROP_HEIGHT, COLOR_BG);
  if (visible) {
    drawIcon(ICON_DROP_X, ICON_DROP_Y, ICON_DROP, ICON_DROP_WIDTH, ICON_DROP_HEIGHT);
    tft.setFont(FONT_TAG);
    tft.setTextColor(COLOR_TAG);   // drawNumber() sets COLOR_TEXT again before the next value
    tft.setCursor(ICON_DROP_X + MODE_LETTER_DX, ICON_DROP_Y + MODE_LETTER_DY);
    tft.print(manual ? F("<") : F("4"));   // FontTag codes: '4' = A, '<' = M
  }
}
#else
void drawDrop(bool, bool) {}
void beginWelcome() {}
void endWelcome() {}
#endif  // DISPLAY_ICONS

// ------------------------------------------------------------
// Values

static const uint8_t *glyphBitmap(const GFXfont *font) {
#ifdef __AVR__
  return (const uint8_t *)pgm_read_word(&font->bitmap);
#else
  return font->bitmap;
#endif
}

// A glyph of a digit font. The fonts are in flash on the AVR: their fields are read with pgm_read_*.
// All the fonts start at '0'.
static const GFXglyph *glyphOf(const GFXfont *font, char c) {
#ifdef __AVR__
  const GFXglyph *glyphs = (const GFXglyph *)pgm_read_word(&font->glyph);
#else
  const GFXglyph *glyphs = font->glyph;
#endif
  return &glyphs[c - '0'];
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

// Draws one character as a block of the zone height, in one burst: the pixels of the glyph in the text color
// and all the others in the background color. The old character is replaced at once, without a moment where
// the place is blank (what a erase followed by a print would show).
// The block is as wide as the cell of the character, and the glyph is centered in it: no glyph goes beyond its
// cell (checked for all the digits of the two fonts), so the blocks never overlap.
static void drawGlyphBlock(const GFXfont *font, char c, int16_t x, int16_t y, int16_t height, int16_t baseline) {
  const GFXglyph *glyph = glyphOf(font, c);
  const uint8_t width = pgm_read_byte(&glyph->width);
  const uint8_t rows = pgm_read_byte(&glyph->height);
  // The glyph is centered in its (wider) cell
  const int16_t xOffset = (int8_t)pgm_read_byte(&glyph->xOffset)
                          + (advanceOf(font, c) - pgm_read_byte(&glyph->xAdvance)) / 2;
  const int16_t top = baseline + (int8_t)pgm_read_byte(&glyph->yOffset);   // first row of the glyph
  const uint8_t *bits = glyphBitmap(font) + pgm_read_word(&glyph->bitmapOffset);

  const uint8_t blockWidth = advanceOf(font, c);

  tft.startWrite();
  tft.setAddrWindow(x, y, blockWidth, height);

  // The pixels are sent in runs of the same color
  uint16_t runColor = COLOR_BG;
  uint16_t runLength = 0;
  for (int16_t row = 0; row < height; row++) {
    const int16_t glyphRow = y + row - top;
    for (int16_t column = 0; column < blockWidth; column++) {
      const int16_t glyphColumn = column - xOffset;
      uint16_t color = COLOR_BG;
      if (glyphRow >= 0 && glyphRow < rows && glyphColumn >= 0 && glyphColumn < width) {
        const uint16_t bit = glyphRow * width + glyphColumn;
        if (pgm_read_byte(&bits[bit >> 3]) & (0x80 >> (bit & 7))) {
          color = COLOR_TEXT;
        }
      }
      if (color != runColor && runLength) {
        tft.writeColor(runColor, runLength);
        runLength = 0;
      }
      runColor = color;
      runLength++;
    }
  }
  tft.writeColor(runColor, runLength);
  tft.endWrite();
}

// Draws a number (small or large digits) left-aligned at x, on the given baseline. Every digit is a block that
// replaces what was there (see drawGlyphBlock), so nothing is erased first and nothing flickers, even if the
// digits are the same as before. The rest of the zone is erased only where it is needed:
//   - left of the number and right of it, when the field did not show the same look before (first draw, other
//     digit size, other tag) or when force is set;
//   - right of the number, when it ends elsewhere than before (shorter number...).
// moved is set when the end of the number is not where it was: the unit that follows must be drawn again.
// Returns the x position after the number, where the unit goes.
static int16_t drawNumber(NumberField &field, const char *text, TextSize size, PhaseTag tag, bool force,
                          int16_t zoneX, int16_t zoneY, int16_t zoneWidth, int16_t zoneHeight,
                          int16_t x, int16_t baseline, bool &moved) {
  // Three digits do not fit in large digits (the cells are wide): a long value is shown in small digits
  if (strlen(text) > 2) {
    size = TEXT_SIZE_SMALL;
  }

  const GFXfont *font = (size == TEXT_SIZE_SMALL) ? FONT_SMALL : FONT_LARGE;
  const bool sameLook = field.valid && !force && field.size == size && field.tag == tag;

  // One block per digit, from left to right
  int16_t xEnd = x;
  for (const char *c = text; *c; c++) {
    drawGlyphBlock(font, *c, xEnd, zoneY, zoneHeight, baseline);
    xEnd += advanceOf(font, *c);
  }

  moved = !sameLook || xEnd != field.xEnd;

  // Around the blocks: the left margin of the zone (new look), and the right of the number, up to the end of the
  // zone, if it moved (the old one was longer, the unit is going to be drawn again)
  if (!sameLook && x > zoneX) {
    tft.fillRect(zoneX, zoneY, x - zoneX, zoneHeight, COLOR_BG);
  }
  if (moved && xEnd < zoneX + zoneWidth) {
    tft.fillRect(xEnd, zoneY, zoneX + zoneWidth - xEnd, zoneHeight, COLOR_BG);
  }

  tft.setTextColor(COLOR_TEXT);   // for the unit that follows (the tag drawn last changed the color)

  field.xEnd = xEnd;
  field.size = size;
  field.tag = tag;
  field.valid = true;
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
    tft.setFont(FONT_UNIT);
    tft.setCursor(xNumberEnd + TEMPERATURE_DEGREE_DX, TEMPERATURE_DEGREE_Y);
    tft.print('4');
    tft.setCursor(xNumberEnd + TEMPERATURE_UNIT_DX, TEMPERATURE_UNIT_BASELINE);
    tft.print('0');   // FontUnit codes: '0' = C, '2' '1' = ml, '3' = s (see Fonts.h)
  }
}

// "ERR" in white, twice the size of the tag letters: FontTag ('2' = E, '1' = R) has the only capitals
void drawTemperatureError() {
  tft.fillRect(TEMPERATURE_ZONE_X, TEMPERATURE_ZONE_Y, TEMPERATURE_ZONE_WIDTH, TEMPERATURE_ZONE_HEIGHT, COLOR_BG);
  temperatureField.valid = false;   // the temperature has to be drawn again in full
  tft.setFont(FONT_TAG);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setCursor(TEMPERATURE_X, TEMPERATURE_ERROR_BASELINE);
  tft.print(F("211"));
  tft.setTextSize(1);   // the other texts are drawn at size 1
}

// Phase tag at a given position. FontTag only holds the letters P R E W A I T F L U S H, under the
// codes '0'..';' (see Fonts.h): "012" is PRE, "3456" is WAIT and "789:;" is FLUSH.
static void drawTag(PhaseTag tag, int16_t x, int16_t baseline) {
  tft.setFont(FONT_TAG);
  tft.setTextColor(COLOR_TAG);   // drawNumber() sets COLOR_TEXT again before the next value
  tft.setCursor(x, baseline);
  // FontTag codes: P R E = "012", C A L = "=48", F L U S H = "789:;", W A I T = "3456"
  tft.print(tag == PHASE_TAG_PREINFUSION ? F("012") :
            (tag == PHASE_TAG_CALIBRATION ? F("=48") : (tag == PHASE_TAG_FLUSH ? F("789:;") : F("3456"))));
}

// Volume in ml, with its tag (PRE, CAL...) when there is one. The tag is redrawn only if redrawTag.
void drawVolume(uint16_t value, TextSize size, PhaseTag tag, bool redrawTag) {
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
                                        VOLUME_X, VOLUME_BASELINE, moved);

  if (moved) {
    // The "ml" (small or large digits) can reach under the tag, in a part of the field that the zone
    // above does not cover: erased too, below the tag.
    if (keepTag) {
      tft.fillRect(VOLUME_TAG_X, VOLUME_TAG_BASELINE + 1,
                   VOLUME_ZONE_X + VOLUME_ZONE_WIDTH - VOLUME_TAG_X,
                   VOLUME_ZONE_Y + VOLUME_ZONE_HEIGHT - (VOLUME_TAG_BASELINE + 1), COLOR_BG);
    }

    tft.setFont(FONT_UNIT);
    tft.setCursor(xNumberEnd + VOLUME_UNIT_DX, VOLUME_UNIT_BASELINE);
    tft.print(F("21"));
  }

  if (tag != PHASE_TAG_NONE && redrawTag) {
    drawTag(tag, VOLUME_TAG_X, VOLUME_TAG_BASELINE);
  }
}

// Duration in seconds, with its tag (WAIT, FLUSH) when there is one.
void drawDuration(uint16_t value, TextSize size, PhaseTag tag, bool redrawTag) {
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
                                        DURATION_X, DURATION_BASELINE, moved);

  if (moved) {
    // Large digits: the "s" is under the tag, in a part of the field that the zone above does not cover
    // (also when it was drawn there by the previous large value).
    if (keepTag && (size == TEXT_SIZE_LARGE || wasLarge)) {
      tft.fillRect(DURATION_TAG_X, DURATION_TAG_BASELINE + 1,
                   DURATION_ZONE_X + DURATION_ZONE_WIDTH - DURATION_TAG_X,
                   DURATION_ZONE_Y + DURATION_ZONE_HEIGHT - (DURATION_TAG_BASELINE + 1), COLOR_BG);
    }

    tft.setFont(FONT_UNIT);
    tft.setCursor(xNumberEnd + DURATION_UNIT_DX, DURATION_UNIT_BASELINE);
    tft.print('3');
  }

  if (tag != PHASE_TAG_NONE && redrawTag) {
    drawTag(tag, DURATION_TAG_X, DURATION_TAG_BASELINE);
  }
}

// Volume and duration erased (blinking of the result of an extraction).
void clearValues() {
  tft.fillRect(VOLUME_ZONE_X, VOLUME_ZONE_Y, VOLUME_ZONE_WIDTH, VOLUME_ZONE_HEIGHT, COLOR_BG);
  tft.fillRect(DURATION_ZONE_X, DURATION_ZONE_Y, DURATION_ZONE_WIDTH, DURATION_ZONE_HEIGHT, COLOR_BG);
  volumeField.valid = durationField.valid = false;
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
  tft.setFont(FONT_UNIT);
  tft.setTextColor(visible ? COLOR_TEXT : COLOR_BG);
  tft.setCursor(TEMPERATURE_DOT_X, TEMPERATURE_DOT_Y);
  tft.print('5');
}
