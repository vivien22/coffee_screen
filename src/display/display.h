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
#include "../config.h"

// Size of the digits of a value
enum TextSize {
  TEXT_SIZE_SMALL,
  TEXT_SIZE_LARGE
};

// Phase tag drawn in the volume field ("PRE": the volume then counts down the millilitres left
// to send) or in the duration field ("WAIT", "FLUSH": the duration then counts down the seconds
// left in the phase).
enum PhaseTag : uint8_t {
  PHASE_TAG_NONE,              // nothing: normal extraction or no extraction
  PHASE_TAG_PREINFUSION,       // "PRE":  the puck is being wetted (volume field)
  PHASE_TAG_PREINFUSION_WAIT,  // "WAIT": the puck is soaking (duration field)
  PHASE_TAG_FLUSH              // "FLUSH": the group head is rinsed, or waits to be (duration field)
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
// tag: top right of the field, above the unit (PHASE_TAG_PREINFUSION in the volume field).
// redrawTag false: only the number and its unit are erased and redrawn, the tag already on the
// screen is left as it is (use it when only the number changed). true: the whole field is redrawn.
void drawVolume(uint16_t value, TextSize size, PhaseTag tag, bool redrawTag);
// tag: top right of the field, above the unit, whatever the size of the digits.
// redrawTag false: only the number and its unit are erased and redrawn, the tag already on the
// screen is left as it is (use it when only the number changed). true: the whole field is redrawn.
void drawDuration(uint16_t value, TextSize size, PhaseTag tag, bool redrawTag);

// Show or hide the phase tag of the duration field alone (blinking tag): drawn again at its
// place, or erased.
void drawPhaseTag(PhaseTag tag, bool visible);

// Erase the zone of a value
void clearTemperature();
void clearVolume();
void clearDuration();

// Dot next to the temperature, visible while the heater is on
void drawHeaterIndicator(bool visible);
