#pragma once
#include "Arduino.h"
struct GFXglyph { uint16_t bitmapOffset; uint8_t width,height,xAdvance; int8_t xOffset,yOffset; };
struct GFXfont { uint8_t *bitmap; GFXglyph *glyph; uint16_t first,last; uint8_t yAdvance; };
class Adafruit_GFX { public:
 void fillRect(int16_t,int16_t,int16_t,int16_t,uint16_t){} void setTextColor(uint16_t){} void setFont(const GFXfont*){}
 void setCursor(int16_t,int16_t){} template<class T> void print(T){} int16_t getCursorX(){return 0;}
 void fillCircle(int16_t,int16_t,int16_t,uint16_t){} void drawCircle(int16_t,int16_t,int16_t,uint16_t){}
 void startWrite(){} void endWrite(){} void writePixel(int16_t,int16_t,uint16_t){} void fillScreen(uint16_t){} };
