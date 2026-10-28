/*
  Declarations for coffee_screen.ino: pins, settings, fonts and data types.
  Positions and colors (icons and texts) are in icons.h.
*/

#pragma once

#include <Arduino.h>
#include "fonts.h"
#include "icons.h"

// ---------- Pins ----------
#define PIN_TFT_CS       10
#define PIN_TFT_DC       7
#define PIN_TFT_RST      8
#define PIN_TFT_BL       9     // backlight
#define PIN_ENCODER_GA   2     // interrupt pins: D2 and D3 on Nano
#define PIN_ENCODER_GB   3

// ---------- CJMCU-111 rotary encoder (adjusts the selected value) ----------
#define ENCODER_STEP     1     // value change per detent
#define VALUE_MIN        0
#define TEMPERATURE_MAX  199
#define VOLUME_MAX       999
#define DURATION_MAX     999
#define LONG_PUSH_MS     800   // a push held this long (ms) is a long push (reported before the release)
#define DOUBLE_PUSH_MS   350   // max delay (ms) between two short pushes for a double push (0 = disabled)
                               // a short push is reported once this delay has passed

// ---------- Fonts (digits in two sizes, units) ----------
#define FONT_SMALL (&FontSmall)   // digits, small size
#define FONT_LARGE (&FontLarge)   // digits, large size
#define FONT_UNIT  (&FontUnit)    // units: C, ml, s

// ---------- Paremeters EEPROM address ----------
#define PARAMETERS_BASE_ADDR        ((int)0)
#define PARAMETERS_TEMPERATURE_ADDR (PARAMETERS_BASE_ADDR)
#define PARAMETERS_VOLUME_ADDR      (PARAMETERS_TEMPERATURE_ADDR + sizeof(parameters.temperature.value))

// ---------- Types ----------
enum TextSize {
  TEXT_SIZE_SMALL,
  TEXT_SIZE_LARGE
};

// Value adjusted by the encoder (a double push selects the next one)
enum ActiveParameter {
  PARAMETER_VOLUME,
  PARAMETER_TEMPERATURE
};

// One displayed parameter: its value and the size of the digits
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
