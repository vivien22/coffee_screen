/*
  display: drawing of the coffee screen on the GC9A01 round display (240 x 240).

  It draws the three icons and the three parameters shown on the screen:
  temperature (degrees C), volume (ml) and duration (s).

  Positions, colors and icon data are in icons.h, the fonts are in fonts.h
  (both are only used by display.cpp).

  Requires the Adafruit GFX Library, Adafruit GC9A01A and Adafruit BusIO.
*/

#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

// ---------- Pins of the screen ----------
#define PIN_TFT_CS       10
#define PIN_TFT_DC       7
#define PIN_TFT_RST      8
#define PIN_TFT_BL       9     // backlight

// ---------- Types ----------

// Smallest value of a parameter (the largest one is the maxValue of each parameter)
#define VALUE_MIN        0

// Size of the digits of a parameter
enum TextSize {
  TEXT_SIZE_SMALL,
  TEXT_SIZE_LARGE
};

// One displayed parameter: its value, the size of the digits and its maximum value
struct Parameter {
  int value;
  TextSize size;
  int maxValue;

  // Add delta to the value, kept between VALUE_MIN and maxValue.
  // Returns true if the value changed (the parameter must be redrawn).
  bool adjust(int delta) {
    int newValue = constrain(value + delta, VALUE_MIN, maxValue);
    bool changed = (newValue != value);
    value = newValue;
    return changed;
  }
};

// The three parameters shown on the screen
struct ScreenParameters {
  Parameter temperature;   // °C   (0 to TEMPERATURE_MAX)
  Parameter volume;        // ml   (0 to VOLUME_MAX)
  Parameter duration;      // s    (0 to DURATION_MAX)
};

// ---------- Display ----------

// The screen driver, defined in display.cpp
extern Adafruit_GC9A01A tft;

// Initialize the screen and fill it with the background color.
// The backlight stays off until the screen is filled, to avoid artefacts at power-on.
void beginDisplay();

// Helpers: blend the icon color with the background (opacity 0 to 15) and
// draw one icon stored as 4-bit opacity in PROGMEM
uint16_t blendColor(uint8_t opacity);
void drawIcon(int16_t x, int16_t y, const uint8_t *opacities, uint8_t width, uint8_t height);

// Draw the three icons (thermometer, drop, clock)
void drawIcons();

// Draw one parameter: the zone is erased, then the value and its unit are drawn
void drawTemperature(const Parameter &parameter);
void drawVolume(const Parameter &parameter);
void drawDuration(const Parameter &parameter);

// Draw the three parameters
void drawParameters(const ScreenParameters &parameters);

void blinkTemperature(const Parameter &parameter);

void blinkVolume(const Parameter &parameter);

void blinkPIDOutputIndicator(bool visible);
