/*
  display: drawing of the coffee screen (see display.h).
*/

#include "display.h"
#include "fonts.h"
#include "icons.h"

// ---------- Fonts (digits in two sizes, units) ----------
#define FONT_SMALL (&FontSmall)   // digits, small size
#define FONT_LARGE (&FontLarge)   // digits, large size
#define FONT_UNIT  (&FontUnit)    // units: C, ml, s, degree sign and heater dot (codes '0'..'5', see fonts.h)
#define FONT_TAG   (&FontTag)     // phase tag: PRE, WAIT (15 px capitals)

// The screen driver
static Adafruit_GC9A01A tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

void beginDisplay() {
  // Set Blk Pin of the screen to Low in order to avoid artefacts before the screen is filled of black
  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, LOW);

  tft.begin();
  tft.fillScreen(COLOR_BG);

  // Set Blk Pin to High
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

// Draw an icon stored as 2-bit opacity (PROGMEM): four pixels per byte, the first pixel in the
// high bits. Level 0 (background) is not drawn: the screen is already filled with COLOR_BG.
// Levels 1, 2, 3 are 5/15, 10/15, 15/15 of the icon color.
static void drawIcon(int16_t x, int16_t y, const uint8_t *levels, uint8_t width, uint8_t height) {
  tft.startWrite();
  for (uint16_t i = 0; i < (uint16_t)width * height; i++) {
    const uint8_t level = (pgm_read_byte(&levels[i >> 2]) >> (6 - 2 * (i & 3))) & 0x03;
    if (level) {
      tft.writePixel(x + i % width, y + i / width,
                     level == 3 ? BLEND_COLOR(15) : (level == 2 ? BLEND_COLOR(10) : BLEND_COLOR(5)));
    }
  }
  tft.endWrite();
}

void drawIcons() {
  drawIcon(ICON_THERMOMETER_X, ICON_THERMOMETER_Y, ICON_THERMOMETER, ICON_THERMOMETER_WIDTH, ICON_THERMOMETER_HEIGHT);
  drawIcon(ICON_DROP_X,        ICON_DROP_Y,        ICON_DROP,        ICON_DROP_WIDTH,        ICON_DROP_HEIGHT);
  drawIcon(ICON_CLOCK_X,       ICON_CLOCK_Y,       ICON_CLOCK,       ICON_CLOCK_WIDTH,       ICON_CLOCK_HEIGHT);
}

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
#endif  // DISPLAY_ICONS

// ------------------------------------------------------------
// Values

// Erase the zone of a value, then draw it (small or large digits) left-aligned
// at x, on the given baseline. Returns the x position after the number, where the unit goes.
static int16_t drawNumber(int16_t value,
                          TextSize size,
                          int16_t zoneX, int16_t zoneY, int16_t zoneWidth, int16_t zoneHeight,
                          int16_t x, int16_t baseline) {
  char text[6];
  itoa(value, text, 10);

  tft.fillRect(zoneX, zoneY, zoneWidth, zoneHeight, COLOR_BG);
  tft.setTextColor(COLOR_TEXT);
  if (size == TEXT_SIZE_SMALL) {
    tft.setFont(FONT_SMALL);
  } else {
    tft.setFont(FONT_LARGE);
  }
  tft.setCursor(x, baseline);
  tft.print(text);

  return tft.getCursorX();
}

void drawTemperature(double value, TextSize size) {
  // Display one decimal below 100 °C; use an integer at 100 °C and above.
  const uint16_t roundedTenths = (uint16_t)(value * 10.0 + 0.5);
  const bool showDecimal = (value < 100 && roundedTenths < 1000);
  const uint16_t integerValue = showDecimal
    ? roundedTenths / 10
    : (uint16_t)(value + 0.5);

  int16_t xNumberEnd = drawNumber(integerValue,
                                  size,
                                  TEMPERATURE_ZONE_X, TEMPERATURE_ZONE_Y, TEMPERATURE_ZONE_WIDTH, TEMPERATURE_ZONE_HEIGHT,
                                  TEMPERATURE_X, TEMPERATURE_BASELINE);

  if (showDecimal && size == TEXT_SIZE_SMALL) {
    // Decimal point and first decimal digit, in the digit font still selected by drawNumber():
    // the decimal point is the glyph ':' of FontSmall (see fonts.h).
    const char decimal[3] = { ':', (char)('0' + (roundedTenths % 10)), 0 };
    tft.setCursor(xNumberEnd, TEMPERATURE_BASELINE);
    tft.print(decimal);
    xNumberEnd = tft.getCursorX();
  }

  // Degree sign (glyph '4' of FontUnit, centered on the cursor), then the unit
  tft.setFont(FONT_UNIT);
  tft.setCursor(xNumberEnd + TEMPERATURE_DEGREE_DX, TEMPERATURE_DEGREE_Y);
  tft.print('4');
  tft.setCursor(xNumberEnd + TEMPERATURE_UNIT_DX, TEMPERATURE_UNIT_BASELINE);
  tft.print('0');   // FontUnit codes: '0' = C, '2' '1' = ml, '3' = s (see fonts.h)
}

// "ERR" in white, twice the size of the tag letters: FontTag ('2' = E, '1' = R) has the only capitals
void drawTemperatureError() {
  tft.fillRect(TEMPERATURE_ZONE_X, TEMPERATURE_ZONE_Y, TEMPERATURE_ZONE_WIDTH, TEMPERATURE_ZONE_HEIGHT, COLOR_BG);
  tft.setFont(FONT_TAG);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.setCursor(TEMPERATURE_X, TEMPERATURE_ERROR_BASELINE);
  tft.print(F("211"));
  tft.setTextSize(1);   // the other texts are drawn at size 1
}

// Phase tag at a given position. FontTag only holds the letters P R E W A I T F L U S H, under the
// codes '0'..';' (see fonts.h): "012" is PRE, "3456" is WAIT and "789:;" is FLUSH.
static void drawTag(PhaseTag tag, int16_t x, int16_t baseline) {
  tft.setFont(FONT_TAG);
  tft.setTextColor(COLOR_TAG);   // drawNumber() sets COLOR_TEXT again before the next value
  tft.setCursor(x, baseline);
  tft.print(tag == PHASE_TAG_PREINFUSION ? F("012") : (tag == PHASE_TAG_FLUSH ? F("789:;") : F("3456")));
}

void drawVolume(uint16_t value, TextSize size, PhaseTag tag, bool redrawTag) {
  // The tag already drawn is kept when only the number changes: the zone erased before the
  // number stops where the tag starts.
  const bool keepTag = (tag != PHASE_TAG_NONE) && !redrawTag;
  int16_t zoneWidth = VOLUME_ZONE_WIDTH;
  if (keepTag) {
    zoneWidth = VOLUME_TAG_X - VOLUME_ZONE_X;
  }

  int16_t xNumberEnd = drawNumber(value,
                                  size,
                                  VOLUME_ZONE_X, VOLUME_ZONE_Y, zoneWidth, VOLUME_ZONE_HEIGHT,
                                  VOLUME_X, VOLUME_BASELINE);

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

  if (tag != PHASE_TAG_NONE && redrawTag) {
    drawTag(tag, VOLUME_TAG_X, VOLUME_TAG_BASELINE);
  }
}

void drawDuration(uint16_t value, TextSize size, PhaseTag tag, bool redrawTag) {
  // The tag already drawn is kept when only the number changes: the zone erased before the
  // number stops where the tag starts.
  const bool keepTag = (tag != PHASE_TAG_NONE) && !redrawTag;
  int16_t zoneWidth = DURATION_ZONE_WIDTH;
  if (keepTag) {
    zoneWidth = DURATION_TAG_X - DURATION_ZONE_X;
  }

  int16_t xNumberEnd = drawNumber(value,
                                  size,
                                  DURATION_ZONE_X, DURATION_ZONE_Y, zoneWidth, DURATION_ZONE_HEIGHT,
                                  DURATION_X, DURATION_BASELINE);

  // Large digits: the "s" is under the tag, in a part of the field that the zone above does not cover.
  if (keepTag && size == TEXT_SIZE_LARGE) {
    tft.fillRect(DURATION_TAG_X, DURATION_TAG_BASELINE + 1,
                 DURATION_ZONE_X + DURATION_ZONE_WIDTH - DURATION_TAG_X,
                 DURATION_ZONE_Y + DURATION_ZONE_HEIGHT - (DURATION_TAG_BASELINE + 1), COLOR_BG);
  }

  tft.setFont(FONT_UNIT);
  tft.setCursor(xNumberEnd + DURATION_UNIT_DX, DURATION_UNIT_BASELINE);
  tft.print('3');

  if (tag != PHASE_TAG_NONE && redrawTag) {
    drawTag(tag, DURATION_TAG_X, DURATION_TAG_BASELINE);
  }
}

void drawPhaseTag(PhaseTag tag, bool visible) {
  if (visible) {
    drawTag(tag, DURATION_TAG_X, DURATION_TAG_BASELINE);
  } else {
    // Only the rows of the letters: the number and its unit are not touched
    tft.fillRect(DURATION_TAG_X, DURATION_TAG_BASELINE - DURATION_TAG_HEIGHT,
                 DURATION_ZONE_X + DURATION_ZONE_WIDTH - DURATION_TAG_X, DURATION_TAG_HEIGHT, COLOR_BG);
  }
}

void clearTemperature() {
  tft.fillRect(TEMPERATURE_ZONE_X, TEMPERATURE_ZONE_Y, TEMPERATURE_ZONE_WIDTH, TEMPERATURE_ZONE_HEIGHT, COLOR_BG);
}

void clearVolume() {
  tft.fillRect(VOLUME_ZONE_X, VOLUME_ZONE_Y, VOLUME_ZONE_WIDTH, VOLUME_ZONE_HEIGHT, COLOR_BG);
}

void clearDuration() {
  tft.fillRect(DURATION_ZONE_X, DURATION_ZONE_Y, DURATION_ZONE_WIDTH, DURATION_ZONE_HEIGHT, COLOR_BG);
}

void drawHeaterIndicator(bool visible) {
  // Glyph '5' of FontUnit, centered on the cursor: drawn in the background color it erases the dot
  tft.setFont(FONT_UNIT);
  tft.setTextColor(visible ? COLOR_TEXT : COLOR_BG);
  tft.setCursor(TEMPERATURE_DOT_X, TEMPERATURE_DOT_Y);
  tft.print('5');
}
