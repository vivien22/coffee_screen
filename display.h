/*
  display: drawing of the coffee screen on the GC9A01 round display (240 x 240).

  It only draws what it is given: the three values shown on the screen
  (temperature in degrees C, volume in ml, duration in s) and the heater indicator.
  It knows nothing about the machine; what to show and when is decided by screen_controller.

  Positions, colors and icon data are in icons.h, the fonts are in fonts.h
  (both are only used by display.cpp), the pins are in config.h.

  Requires the Adafruit GFX Library and Adafruit GC9A01A.
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

// Phase tag drawn after the duration while the pre-infusion is in progress
// (the duration then counts down the seconds left in the phase).
enum PhaseTag : uint8_t {
  PHASE_TAG_NONE,              // nothing: normal extraction or no extraction
  PHASE_TAG_PREINFUSION,       // "PRE":  the puck is being wetted
  PHASE_TAG_PREINFUSION_WAIT   // "WAIT": the puck is soaking
};

// Initialize the screen and fill it with the background color.
// The backlight stays off until the screen is filled, to avoid artefacts at power-on.
void beginDisplay();

#ifdef DISPLAY_ICONS
// Draw the three icons (thermometer, drop, clock)
void drawIcons();
#endif

// Draw one value: its zone is erased, then the value and its unit are drawn
void drawTemperature(double value, TextSize size);
void drawVolume(uint16_t value, TextSize size);
void drawDuration(uint16_t value, TextSize size, PhaseTag tag);   // tag is drawn after the unit

// Erase the zone of a value
void clearTemperature();
void clearVolume();

// Dot next to the temperature, visible while the heater is on
void drawHeaterIndicator(bool visible);
