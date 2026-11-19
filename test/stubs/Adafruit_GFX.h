// Adafruit_GFX stand-in for the host: draws into a 240 x 240 RGB565 frame buffer, with the real GFX font rules
// (bit-packed glyphs, offsets, advance). Used by tools/render_screens.cpp (the pictures of the README) and by
// test/test_display.cpp. A program that links Display.cpp must define frameBuffer.
#pragma once
#include "Arduino.h"

struct GFXglyph { uint16_t bitmapOffset; uint8_t width, height, xAdvance; int8_t xOffset, yOffset; };
struct GFXfont { uint8_t *bitmap; GFXglyph *glyph; uint16_t first, last; uint8_t yAdvance; };

extern uint16_t frameBuffer[240 * 240];

class Adafruit_GFX {
  public:
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
    // The same, from an array of colors (Adafruit_SPITFT::writePixels)
    void writePixels(uint16_t *colors, uint32_t len, bool = true, bool = false) {
      for (; len; len--, wi_++) writePixel(wx_ + wi_ % ww_, wy_ + wi_ / ww_, *colors++);
    }
    void setFont(const GFXfont *f) { font_ = f; }
    void setTextColor(uint16_t c) { color_ = c; }
    void setTextSize(uint8_t s) { size_ = s; }
    void setCursor(int16_t x, int16_t y) { cx_ = x; cy_ = y; }
    int16_t getCursorX() { return cx_; }
    void print(char c) {
      if (!font_ || c < font_->first || c > font_->last) return;
      const GFXglyph &g = font_->glyph[c - font_->first];
      const uint8_t *bitmap = font_->bitmap + g.bitmapOffset;
      uint16_t bit = 0;
      for (uint8_t yy = 0; yy < g.height; yy++) {
        for (uint8_t xx = 0; xx < g.width; xx++, bit++) {
          if (bitmap[bit >> 3] & (0x80 >> (bit & 7))) {
            fillRect(cx_ + (g.xOffset + xx) * size_, cy_ + (g.yOffset + yy) * size_, size_, size_, color_);
          }
        }
      }
      cx_ += g.xAdvance * size_;
    }
    void print(const char *s) { while (*s) print(*s++); }
    // Not used by the display code, only to build with it
    void fillCircle(int16_t, int16_t, int16_t, uint16_t) {}
    void drawCircle(int16_t, int16_t, int16_t, uint16_t) {}
  private:
    const GFXfont *font_ = nullptr;
    uint16_t color_ = 0xFFFF;
    uint8_t size_ = 1;
    int16_t cx_ = 0, cy_ = 0;
    uint16_t wx_ = 0, wy_ = 0, ww_ = 1, wh_ = 1;
    uint32_t wi_ = 0;
};
