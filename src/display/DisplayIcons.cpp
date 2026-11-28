// DisplayIcons.cpp: the icons and the pictures of the display (compressed, see IconsData.h), see Display.h.

#include "Display.h"
#include "DisplayText.h"
#include "Layout.h"
#include "Screen.h"

#ifdef DISPLAY_ICONS
// ------------------------------------------------------------
// Icons

// COLOR_ICON blended with COLOR_BG: BLEND_COLOR() in DisplayText.h

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

// Draw an icon stored as 4-bit opacity (16 levels), compressed (PROGMEM), row by row. The data is a stream of tokens
// (tools/compress_icons.cpp writes it from tools/icons_raw.h):
// the first two bits are the kind of the token:
//     00 nnnnn           nnnnn + 1 pixels of level 0 (the background)
//     11 nnnn            nnnn + 1 pixels of level 15 (the full color)
//     01 llll            one pixel of level llll (an edge pixel)
//     10 llll mmmmmm     mmmmmm + 1 pixels of level llll (the large areas of the welcome picture)
// Level 0 (background) is not drawn: the screen is already filled with COLOR_BG. Level l is l/15 of the icon color.
//
// A run is drawn as one burst of one color (a window of one row, then the pixels), not pixel by pixel: setting
// a window costs more than a pixel, so the runs of the icons and of the welcome picture are drawn several times
// faster. A run that reaches the end of a row goes on in the next one (a burst per row).
static void drawIcon(int16_t x, int16_t y, const uint8_t *data, uint8_t width, uint8_t height) {
  IconBits bits = {data, 0, 0};
  uint8_t row = 0, column = 0;   // the next pixel of the icon

  tft.startWrite();
  while (row < height) {
    const uint8_t kind = bits.get(2);
    uint8_t level = 0, run = 1;
    if (kind == 0) run = bits.get(5) + 1;
    else if (kind == 3) { level = 15; run = bits.get(4) + 1; }
    else {
      level = bits.get(4);
      if (kind == 2) run = bits.get(6) + 1;
    }

    while (run) {
      const uint8_t part = (run < width - column) ? run : (uint8_t)(width - column);   // up to the end of the row
      if (level) {
        tft.setAddrWindow(x + column, y + row, part, 1);
        const uint16_t color = BLEND_COLOR(level);
        tft.pushColor(textBrightness == 255 ? color : dimColor(color), part);
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
  drawIcon(WELCOME_X, WELCOME_Y, ICON_WELCOME, ICON_WELCOME_WIDTH, ICON_WELCOME_HEIGHT);   // drawn in the dark
  // The backlight rises to full in BACKLIGHT_FADE_IN_MS, like the other fades of the backlight (same curve). This runs in setup(), where the
  // loop does not exist yet and finishWelcome() waits anyway: a plain delay() is fine here (a fixed number of steps, no clock needed).
  const uint8_t steps = (uint8_t)(BACKLIGHT_FADE_IN_MS / 5UL > 100UL ? 100UL : BACKLIGHT_FADE_IN_MS / 5UL);
  for (uint8_t step = 1; step < steps; step++) {
    setBacklight((uint8_t)((uint16_t)step * 255 / steps));
    delay(BACKLIGHT_FADE_IN_MS / steps);
  }
  setBacklight(255);
#endif
}

// The end of the welcome picture: the backlight goes off and only the square of the picture is cleared (the rest of
// the screen is already clear: 6 times less to send than a whole screen, and nothing is seen anyway). The real screen
// is then drawn in the dark, and shown at once by showDisplay().
void endWelcome() {
#if WELCOME_MS > 0
  setBacklight(0);
  tft.fillRect(WELCOME_X, WELCOME_Y, ICON_WELCOME_WIDTH, ICON_WELCOME_HEIGHT, COLOR_BG);
#endif
}

// The standby: the welcome picture replaces the whole screen. The screen is cleared and the picture drawn without touching the
// backlight: the caller has switched it off (ScreenController, fade) and lights it again. The values that were on the screen are gone: the
// number fields are not valid any more, so the real screen is drawn in full when it comes back.
void showStandbyPicture() {
#if WELCOME_MS > 0
  clearScreen();
  drawIcon(WELCOME_X, WELCOME_Y, ICON_WELCOME, ICON_WELCOME_WIDTH, ICON_WELCOME_HEIGHT);
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
