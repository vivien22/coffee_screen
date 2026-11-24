// Adafruit_GFX stand-in for the host: draws into a 240 x 240 RGB565 frame buffer. It only has what Display.cpp calls: rectangles,
// and the window and bursts of the driver (the texts are drawn by Display.cpp itself, from the masks of Fonts.h). Used by
// tools/render_screens.cpp (the pictures of the documentation) and by test/test_display.cpp. A program that links Display.cpp must
// define frameBuffer.
#pragma once
#include "Arduino.h"

struct GFXglyph { uint16_t bitmapOffset; uint8_t width, height, xAdvance; int8_t xOffset, yOffset; };
struct GFXfont { uint8_t *bitmap; GFXglyph *glyph; uint16_t first, last; uint8_t yAdvance; };

extern uint16_t frameBuffer[240 * 240];

class Adafruit_GFX {
  public:
    virtual size_t write(uint8_t) { return 0; }   // as Print::write: Display.cpp replaces it (see class Screen)
    void fillScreen(uint16_t color) { fillRect(0, 0, 240, 240, color); }
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
      for (int16_t j = y; j < y + h; j++) for (int16_t i = x; i < x + w; i++) writePixel(i, j, color);
    }
    void startWrite() {}
    void endWrite() {}
    void writePixel(int16_t x, int16_t y, uint16_t color) {
      if (x >= 0 && x < 240 && y >= 0 && y < 240) frameBuffer[y * 240 + x] = color;
    }
    // Window and bursts of the driver (Adafruit_SPITFT): the pixels fill the window row by row
    void setAddrWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) { wx_ = x; wy_ = y; ww_ = w; wh_ = h; wi_ = 0; }
    void writeColor(uint16_t color, uint32_t len) {
      for (; len; len--, wi_++) writePixel(wx_ + wi_ % ww_, wy_ + wi_ / ww_, color);
    }
    void writePixels(uint16_t *colors, uint32_t len, bool = true, bool = false) {
      for (uint32_t i = 0; i < len; i++, wi_++) writePixel(wx_ + wi_ % ww_, wy_ + wi_ / ww_, colors[i]);
    }
  private:
    uint16_t wx_ = 0, wy_ = 0, ww_ = 1, wh_ = 1;
    uint32_t wi_ = 0;
};
