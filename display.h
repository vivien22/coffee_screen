/*
  display: drawing of the coffee screen on the GC9A01 round display (240 x 240).

  It only draws what it is given: the three values shown on the screen
  (temperature in degrees C, volume in ml, duration in s) and the heater indicator.
  It knows nothing about the machine; what to show and when is decided by screen_controller.

  Positions, colors and icon data are in icons.h, the fonts are in fonts.h
  (both are only used by display.cpp), the pins are in config.h.

  Requires the Adafruit GFX Library, Adafruit GC9A01A and Adafruit BusIO.
*/

#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include "config.h"

// Size of the digits of a value
enum TextSize {
  TEXT_SIZE_SMALL,
  TEXT_SIZE_LARGE
};

// Initialize the screen and fill it with the background color.
// The backlight stays off until the screen is filled, to avoid artefacts at power-on.
void beginDisplay();

// Draw the three icons (thermometer, drop, clock)
void drawIcons();

// Draw one value: its zone is erased, then the value and its unit are drawn
void drawTemperature(double value, TextSize size);
void drawVolume(uint16_t value, TextSize size);
void drawDuration(uint16_t value, TextSize size);

// Erase the zone of a value
void clearTemperature();
void clearVolume();

// Dot next to the temperature, visible while the heater is on
void drawHeaterIndicator(bool visible);
