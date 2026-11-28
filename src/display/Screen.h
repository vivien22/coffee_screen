/*
  Screen: the GC9A01 driver of the display, with a fast pixel output (see below). Used by the files of the display only
  (Display.cpp, DisplayText.cpp, DisplayIcons.cpp): the rest of the firmware draws through Display.h.
*/

#pragma once
#include <SPI.h>
#include <Adafruit_GC9A01A.h>

// The screen driver. Its text engine (Print::write -> Adafruit_GFX::drawChar, and the default 5 x 7 font) is not used: every text
// is drawn by putText() below. Overriding write() with nothing keeps that code, and the 1280 bytes of the default font, out of
// the firmware (a virtual function of the library is linked even if nobody calls it, unless a class replaces it).
class Screen : public Adafruit_GC9A01A {
  public:
    Screen(int8_t cs, int8_t dc, int8_t rst) : Adafruit_GC9A01A(cs, dc, rst) {}
    size_t write(uint8_t) override { return 0; }

    // Fast pixel output (board only). On this core Adafruit_SPITFT has no fast path: writePixels() and writeColor() (and so every fill)
    // send each pixel as two SPI.transfer(uint8_t), each one waiting for its byte to come back. ArduinoSPI::transfer(buffer, count)
    // sends 32-bit words with two in flight, but it overwrites the buffer with the bytes received, so the buffer is rewritten before
    // every chunk. Called only between startWrite() and endWrite() (CS low, transaction open) and after setAddrWindow() (DC data),
    // like the library's own calls. The host tests keep the library calls (the stub GFX draws into a frame buffer).
    // TO CONFIRM ON THE BOARD: the byte order of transfer(buffer). The core sets BYSW (byte swap) for MSB first, so the bytes should
    // leave in memory order, high byte of the color first as the GC9A01 wants; wrong colors would show at once if not.
#ifdef ARDUINO_STUB
    void pushPixels(const uint16_t *colors, uint32_t count) { writePixels(const_cast<uint16_t *>(colors), count); }
    void pushColor(uint16_t color, uint32_t count) { writeColor(color, count); }
#else
    void pushPixels(const uint16_t *colors, uint32_t count) {
      while (count) {
        const uint16_t part = count < CHUNK_PIXELS ? (uint16_t)count : CHUNK_PIXELS;
        for (uint16_t i = 0; i < part; i++) {
          buffer_[2 * i] = colors[i] >> 8;       // big endian: the high byte first
          buffer_[2 * i + 1] = colors[i] & 0xFF;
        }
        SPI.transfer(buffer_, 2 * part);
        colors += part;
        count -= part;
      }
    }

    void pushColor(uint16_t color, uint32_t count) {
      while (count) {
        const uint16_t part = count < CHUNK_PIXELS ? (uint16_t)count : CHUNK_PIXELS;
        for (uint16_t i = 0; i < part; i++) {    // refilled every chunk: the transfer overwrites the buffer
          buffer_[2 * i] = color >> 8;
          buffer_[2 * i + 1] = color & 0xFF;
        }
        SPI.transfer(buffer_, 2 * part);
        count -= part;
      }
    }

    // As Adafruit_SPITFT::fillRect (same clipping), but the pixels go through pushColor(). fillScreen() of the GFX calls it.
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
      if (w < 0) { x += w + 1; w = -w; }
      if (h < 0) { y += h + 1; h = -h; }
      if (w == 0 || h == 0 || x >= _width || y >= _height) {
        return;
      }
      const int16_t x2 = x + w - 1, y2 = y + h - 1;
      if (x2 < 0 || y2 < 0) {
        return;
      }
      if (x < 0) { x = 0; w = x2 + 1; }
      if (y < 0) { y = 0; h = y2 + 1; }
      if (x2 >= _width) { w = _width - x; }
      if (y2 >= _height) { h = _height - y; }
      startWrite();
      setAddrWindow(x, y, w, h);
      pushColor(color, (uint32_t)w * h);
      endWrite();
    }

  private:
    static const uint16_t CHUNK_PIXELS = 128;                 // 256 bytes per transfer
    uint8_t buffer_[2 * CHUNK_PIXELS] __attribute__((aligned(4))) = {};   // the core reads it as 32-bit words
#endif
};

extern Screen tft;   // the one screen (Display.cpp)
