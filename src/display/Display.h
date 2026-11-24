/*
  display: drawing of the coffee screen on the GC9A01 round display (240 x 240).

  It only draws what it is given: the three values shown on the screen
  (temperature in degrees C, volume in ml, duration in s) and the heater indicator.
  It knows nothing about the machine; what to show and when is decided by ScreenController.

  Positions, colors and icon data are in Layout.h, the fonts are in Fonts.h
  (both are only used by Display.cpp), the pins are in Config.h.

  Requires the Adafruit GFX Library and Adafruit GC9A01A.
*/

#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include "../Config.h"

// The progress of the rolling digits of drawVolume() and drawDuration(): 0 to 254 draws a frame of the roll from the number drawn last
// to the new one, DISPLAY_ROLL_DONE draws the new number as it is (see drawNumber in Display.cpp).
#define DISPLAY_ROLL_DONE 255

// The brightness of the texts drawn from now on (0 to 255, 255 = their color): the colors of the text are blended with COLOR_BG. The icons
// are not texts. Set it back to 255 after a faded drawing (see ScreenController, change of parameter).
void setTextBrightness(uint8_t level);

// The next drawing of a number field (0 = temperature, 1 = volume, 2 = duration) is complete: its zone, its number, its unit and its tag are drawn,
// even if the field already shows the same look. Used by the faded rows: what is on the screen is at another brightness.
void forgetField(uint8_t field);

// The same, but the zone is not erased first: the number, the unit and the tag are drawn over what is there (the frames of a fade at
// another brightness, where an erase followed by a drawing would make the unit and the tag blink).
void repaintField(uint8_t field);

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
  PHASE_TAG_FLUSH,             // "FLUSH": the grouphead is rinsed, or waits to be (duration field)
  PHASE_TAG_CALIBRATION        // "CAL": the weight of the cup is entered (volume field)
  , PHASE_TAG_LIMIT              // "LIMIT": the flow limit of the extraction (volume field, test)
};

// Initialize the screen and fill it with the background color.
// The backlight stays off, to avoid artefacts at power-on and so that the first drawing (icons, values) is not
// seen piece by piece: call showDisplay() once everything is drawn.
void beginDisplay();

// Switch the backlight on: the screen appears complete.
void showDisplay();

// The welcome picture of the start-up: beginWelcome() draws it (right after beginDisplay()) and switches the backlight
// on; it stays until endWelcome() (the caller waits WELCOME_MS between the two, and starts what has to be done in the
// meantime), which switches the backlight off and clears the picture. Both do nothing if WELCOME_MS is 0.
void beginWelcome();
void endWelcome();

// The standby: the welcome picture replaces the whole screen (shown complete, the backlight stays on). endWelcome()
// takes it away, and the caller then draws the real screen again in full. Does nothing if WELCOME_MS is 0.
void showStandbyPicture();

#ifdef DISPLAY_ICONS
// Draw the three icons (thermometer, drop, clock)
void drawIcons();
#endif

// Show or hide the drop icon alone, with the letter of the mode in it ("M" in MANUAL, "A" in AUTO).
// Used at start-up, at a change of mode and for the blinking after a no-flow error.
// Does nothing if DISPLAY_ICONS is not defined.
void drawDrop(bool visible, bool manual);

// Steaming mode: the screen is cleared (everything gone, the fields are drawn again in full afterwards), then only the
// temperature is drawn, in large digits in the middle of the screen. error: "ERR" instead (probe fault).
void clearScreen();
// The decoration of the steaming screen: the thermometer left of the temperature, volutes above it, "STEAM" under it.
// Does nothing if DISPLAY_ICONS is not defined.
void drawSteamIcon();
void drawSteamTemperature(double value, bool error);

// Draw one value: its zone is erased, then the value and its unit are drawn
void drawTemperature(double value, TextSize size);
// Temperature sensor fault: "ERR" replaces the value and its unit
void drawTemperatureError();
// tag: top right of the field, above the unit (PHASE_TAG_PREINFUSION in the volume field).
// redrawTag false: only the number and its unit are erased and redrawn, the tag already on the
// screen is left as it is (use it when only the number changed). true: the whole field is redrawn.
// The flow limit of the extraction, tenths of ml/s shown as "2:5 ml/s" in the volume field, with the tag LIMIT (test)
void drawFlowLimit(uint16_t tenths, bool redrawTag);
// progress: DISPLAY_ROLL_DONE, or a frame of the roll from the number drawn last (the digits that change leave upward, the new ones come
// from below); only when the field shows the same look (size, tag) as before.
void drawVolume(uint16_t value, TextSize size, PhaseTag tag, bool redrawTag, uint8_t progress = DISPLAY_ROLL_DONE);
// tag: top right of the field, above the unit, whatever the size of the digits.
// redrawTag false: only the number and its unit are erased and redrawn, the tag already on the
// screen is left as it is (use it when only the number changed). true: the whole field is redrawn.
void drawDuration(uint16_t value, TextSize size, PhaseTag tag, bool redrawTag, uint8_t progress = DISPLAY_ROLL_DONE);

// Show or hide the phase tag of the duration field alone (blinking tag): drawn again at its
// place, or erased.
void drawPhaseTag(PhaseTag tag, bool visible);

// Dot next to the temperature, visible while the heater is on
void drawHeaterIndicator(bool visible);
