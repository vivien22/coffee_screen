/*
  DisplayText: the text engine of the display. Every text of the screen is drawn by putText(), and every number by drawNumber(): the
  glyphs of Fonts.h (masks of 2 bits per pixel, anti-aliased) blended with the background, sent as blocks that replace what was there.
  The brightness of the texts (setTextBrightness() in Display.h) dims them toward the background (the frames of a fade).

  Used by the files of the display only (Display.cpp, DisplayIcons.cpp): the rest of the firmware draws through Display.h.
*/

#pragma once
#include <Arduino.h>
#include "Display.h"
#include "Fonts.h"

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

// What a number field shows, to erase only what is needed (see drawNumber)
struct NumberField {
  int16_t xEnd;     // where the number ended: the unit follows
  uint8_t size;     // TextSize of the digits
  uint8_t tag;      // PhaseTag shown next to it
  bool valid;       // false: the field is not known to show anything (start, or something else was drawn over it)
  bool repaint;     // the next drawing paints number, unit and tag over what is there, without erasing the zone first (repaintField)
  char text[6];     // the number of the last picture drawn as it is (the old number of a roll), see drawNumber
};

// A glyph of a font. All the fonts start at '0'.
inline const GFXglyph *glyphOf(const GFXfont *font, char c) {
  return &font->glyph[c - '0'];
}

// A text with a font, the cursor at x on the baseline, drawn on the background (see DisplayText.cpp)
void putText(const GFXfont *font, int16_t x, int16_t baseline, uint16_t color, Text text);

// A number in a field (see DisplayText.cpp): returns the x position after it, where the unit goes; moved: the unit must be drawn again
int16_t drawNumber(NumberField &field, const char *text, TextSize size, PhaseTag tag, bool force,
                   int16_t zoneX, int16_t zoneY, int16_t zoneWidth, int16_t zoneHeight,
                   int16_t x, int16_t baseline, bool &moved, bool threeLarge = false, uint8_t progress = DISPLAY_ROLL_DONE);

// The brightness of the texts (setTextBrightness): 255 = full
extern uint8_t textBrightness;

// A color moved toward COLOR_BG by the brightness of the texts (an icon at the brightness of a fade, see drawDropFrame)
uint16_t dimColor(uint16_t color);
