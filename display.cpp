/*
  display: drawing of the coffee screen (see display.h).
*/

#include "display.h"
#include "fonts.h"
#include "icons.h"

// ---------- Fonts (digits in two sizes, units) ----------
#define FONT_SMALL (&FontSmall)   // digits, small size
#define FONT_LARGE (&FontLarge)   // digits, large size
#define FONT_UNIT  (&FontUnit)    // units: C, ml, s

// The screen driver
Adafruit_GC9A01A tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

void beginDisplay() {
  // Set Blk Pin of the screen to Low in order to avoid artefacts before the screen is filled of black
  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, LOW);

  tft.begin();
  tft.fillScreen(COLOR_BG);

  // Set Blk Pin to High
  digitalWrite(PIN_TFT_BL, HIGH);
}

// ------------------------------------------------------------
// Icons

// Blend COLOR_ICON with COLOR_BG; opacity: 0 (background) to 15 (full color).
// The colors are RGB565: 5 bits red, 6 bits green, 5 bits blue.
uint16_t blendColor(uint8_t opacity) {
  // Channels of the background
  uint8_t r = (COLOR_BG >> 11) & 0x1F, g = (COLOR_BG >> 5) & 0x3F, b = COLOR_BG & 0x1F;
  // Difference between the icon color and the background, per channel
  int8_t dr = ((COLOR_ICON >> 11) & 0x1F) - r;
  int8_t dg = ((COLOR_ICON >> 5) & 0x3F) - g;
  int8_t db = (COLOR_ICON & 0x1F) - b;
  // background + difference * opacity / 15, packed back to RGB565
  return ((uint16_t)(r + dr * opacity / 15) << 11) | ((uint16_t)(g + dg * opacity / 15) << 5) | (uint16_t)(b + db * opacity / 15);
}

// Draw an icon stored as 4-bit opacity (PROGMEM): two pixels per byte, the first pixel in the
// high nibble. Background pixels (opacity 0) are not drawn: the screen is already filled with COLOR_BG.
void drawIcon(int16_t x, int16_t y, const uint8_t *opacities, uint8_t width, uint8_t height) {
  tft.startWrite();
  for (uint16_t i = 0; i < (uint16_t)width * height; i++) {
    uint8_t opacity = pgm_read_byte(&opacities[i >> 1]);
    opacity = (i & 1) ? (opacity & 0x0F) : (opacity >> 4);
    if (opacity) tft.writePixel(x + i % width, y + i / width, blendColor(opacity));
  }
  tft.endWrite();
}

void drawIcons() {
  drawIcon(ICON_THERMOMETER_X, ICON_THERMOMETER_Y, ICON_THERMOMETER, ICON_THERMOMETER_WIDTH, ICON_THERMOMETER_HEIGHT);
  drawIcon(ICON_DROP_X,        ICON_DROP_Y,        ICON_DROP,        ICON_DROP_WIDTH,        ICON_DROP_HEIGHT);
  drawIcon(ICON_CLOCK_X,       ICON_CLOCK_Y,       ICON_CLOCK,       ICON_CLOCK_WIDTH,       ICON_CLOCK_HEIGHT);
}

// ------------------------------------------------------------
// Parameters

// Erase the zone of a parameter, then draw its value (small or large digits) left-aligned
// at x, on the given baseline. Returns the x position after the number, where the unit goes.
static int16_t drawNumber(const Parameter &parameter,
                          int16_t value,
                          int16_t zoneX, int16_t zoneY, int16_t zoneWidth, int16_t zoneHeight,
                          int16_t x, int16_t baseline) {
  char text[6];
  itoa(value, text, 10);

  tft.fillRect(zoneX, zoneY, zoneWidth, zoneHeight, COLOR_BG);
  tft.setTextColor(COLOR_TEXT);
  if (parameter.size == TEXT_SIZE_SMALL) {
    tft.setFont(FONT_SMALL);
  } else {
    tft.setFont(FONT_LARGE);
  }
  tft.setCursor(x, baseline);
  tft.print(text);

  return tft.getCursorX();
}

void drawTemperature(const Parameter &parameter, uint16_t value) {
  int16_t xNumberEnd = drawNumber(parameter,
                                  value,
                                  TEMPERATURE_ZONE_X, TEMPERATURE_ZONE_Y, TEMPERATURE_ZONE_WIDTH, TEMPERATURE_ZONE_HEIGHT,
                                  TEMPERATURE_X, TEMPERATURE_BASELINE);

  // Degree sign: the fonts have no such glyph, it is drawn as a ring of 2 px (two circles)
  tft.drawCircle(xNumberEnd + TEMPERATURE_DEGREE_DX, TEMPERATURE_DEGREE_Y, TEMPERATURE_DEGREE_RADIUS, COLOR_TEXT);
  tft.drawCircle(xNumberEnd + TEMPERATURE_DEGREE_DX, TEMPERATURE_DEGREE_Y, TEMPERATURE_DEGREE_RADIUS - 1, COLOR_TEXT);

  tft.setFont(FONT_UNIT);
  tft.setCursor(xNumberEnd + TEMPERATURE_UNIT_DX, TEMPERATURE_UNIT_BASELINE);
  tft.print('C');
}

void drawVolume(const Parameter &parameter, uint16_t value) {
  int16_t xNumberEnd = drawNumber(parameter,
                                  value,
                                  VOLUME_ZONE_X, VOLUME_ZONE_Y, VOLUME_ZONE_WIDTH, VOLUME_ZONE_HEIGHT,
                                  VOLUME_X, VOLUME_BASELINE);

  tft.setFont(FONT_UNIT);
  tft.setCursor(xNumberEnd + VOLUME_UNIT_DX, VOLUME_UNIT_BASELINE);
  tft.print(F("ml"));
}

void drawDuration(const Parameter &parameter, uint16_t value) {
  int16_t xNumberEnd = drawNumber(parameter,
                                  value,
                                  DURATION_ZONE_X, DURATION_ZONE_Y, DURATION_ZONE_WIDTH, DURATION_ZONE_HEIGHT,
                                  DURATION_X, DURATION_BASELINE);

  tft.setFont(FONT_UNIT);
  tft.setCursor(xNumberEnd + DURATION_UNIT_DX, DURATION_UNIT_BASELINE);
  tft.print('s');
}

void drawCurrentParameters(const ScreenParameters &parameters) {
  drawTemperature(parameters.temperature, parameters.temperature.currentValue);
  drawVolume(parameters.volume, parameters.volume.currentValue);
  drawDuration(parameters.duration, parameters.duration.currentValue);
}

void blinkTemperature(const Parameter &parameter) {
  tft.fillRect(TEMPERATURE_ZONE_X, TEMPERATURE_ZONE_Y, TEMPERATURE_ZONE_WIDTH, TEMPERATURE_ZONE_HEIGHT, COLOR_BG);
  delay(500);
  drawTemperature(parameter, parameter.defaultValue);
}

void blinkVolume(const Parameter &parameter) {
  tft.fillRect(VOLUME_ZONE_X, VOLUME_ZONE_Y, VOLUME_ZONE_WIDTH, VOLUME_ZONE_HEIGHT, COLOR_BG);
  delay(500);
  drawVolume(parameter, parameter.defaultValue);
}

void blinkPIDOutputIndicator(bool visible) {
  tft.fillCircle(TEMPERATURE_DOT_X, TEMPERATURE_DOT_Y, TEMPERATURE_DOT_RADIUS, visible ? COLOR_TEXT : COLOR_BG);
}
