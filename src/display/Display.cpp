/*
  display: drawing of the coffee screen (see Display.h). The screen driver is in Screen.h, the text engine in DisplayText.cpp, the icons
  and the pictures in DisplayIcons.cpp; this file draws the fields of the screen.
*/

#include <string.h>
#include "Display.h"
#include "DisplayText.h"
#include "Layout.h"
#include "Screen.h"

Screen tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

static NumberField temperatureField, volumeField, durationField;

// Starts the GC9A01 and clears the screen. The backlight stays off (until showDisplay()): nothing is seen while the
// screen is filled and drawn.
void beginDisplay() {
  // Set Blk Pin of the screen to Low in order to avoid artefacts before the screen is filled of black
  pinMode(PIN_TFT_BL, OUTPUT);
  setBacklight(0);   // from now on the pin is only driven by analogWrite (never digitalWrite: they do not mix)

  tft.begin();
  tft.fillScreen(COLOR_BG);
  temperatureField.valid = volumeField.valid = durationField.valid = false;
}

// Switches the backlight on: the screen appears as it has been drawn.
void showDisplay() {
  setBacklight(255);
}

// The backlight is driven by PWM (D9 is a PWM pin of the Nano R4). The eye does not see the brightness in proportion to the power: the
// level (0 to 255, what the eye should get) is squared to give the duty cycle (quadratic curve, close to a gamma of 2), so that a fade looks
// even and the low levels are not crowded at the end.
void setBacklight(uint8_t level) {
  analogWrite(PIN_TFT_BL, (int)((uint16_t)level * level / 255));
}

// ------------------------------------------------------------
// Values

void forgetField(uint8_t field) {
  (field == 0 ? temperatureField : (field == 1 ? volumeField : durationField)).valid = false;
}

void repaintField(uint8_t field) {
  NumberField &f = (field == 0 ? temperatureField : (field == 1 ? volumeField : durationField));
  f.valid = false;
  f.repaint = true;
}

// Temperature: a decimal is added below 100 degrees (small digits only); the unit follows the number.
void drawTemperature(double value, TextSize size) {
  // Display one decimal below 100 °C; use an integer at 100 °C and above.
  const uint16_t roundedTenths = (uint16_t)(value * 10.0 + 0.5);
  const bool showDecimal = (value < 100 && roundedTenths < 1000);
  const uint16_t integerValue = showDecimal
    ? roundedTenths / 10
    : (uint16_t)(value + 0.5);

  // The decimal point is the glyph ':' of FontSmall (see Fonts.h): the number and its decimal are one text
  char text[6];
  itoa(integerValue, text, 10);
  if (showDecimal && size == TEXT_SIZE_SMALL) {
    const uint8_t length = strlen(text);
    text[length] = ':';
    text[length + 1] = (char)('0' + (roundedTenths % 10));
    text[length + 2] = 0;
  }

  bool moved;
  const int16_t xNumberEnd = drawNumber(temperatureField, text, size, PHASE_TAG_NONE, false,
                                        TEMPERATURE_ZONE_X, TEMPERATURE_ZONE_Y, TEMPERATURE_ZONE_WIDTH, TEMPERATURE_ZONE_HEIGHT,
                                        TEMPERATURE_X, TEMPERATURE_BASELINE, moved);

  if (moved) {
    // Degree sign (glyph '4' of FontUnit, centered on the cursor), then the unit
    putText(FONT_UNIT, xNumberEnd + TEMPERATURE_DEGREE_DX, TEMPERATURE_DEGREE_Y, COLOR_TEXT, F("4"));
    putText(FONT_UNIT, xNumberEnd + TEMPERATURE_UNIT_DX, TEMPERATURE_UNIT_BASELINE, COLOR_TEXT,
            F("0"));   // FontUnit codes: '0' = C, '2' '1' = ml, '3' = s (see Fonts.h)
  }
}

// The whole screen cleared (steaming mode, standby): the number fields are not valid any more.
void clearScreen() {
  tft.fillScreen(COLOR_BG);
  temperatureField.valid = volumeField.valid = durationField.valid = false;
}

// Steaming mode: the temperature in large digits (integer), left-aligned next to the thermometer; the unit follows it.
void drawSteamTemperature(double value, bool error) {
  if (error) {
    tft.fillRect(STEAM_ZONE_X, STEAM_ZONE_Y, STEAM_ZONE_WIDTH, STEAM_ZONE_HEIGHT, COLOR_BG);
    temperatureField.valid = false;
    putText(FONT_ERROR_LARGE, STEAM_ERROR_X, STEAM_ERROR_BASELINE, COLOR_TEXT, F("011"));   // FontError codes: E R R
    return;
  }
  char text[6];
  itoa(value < 0 ? 0 : (int)(value + 0.5), text, 10);
  bool moved;
  const int16_t xEnd = drawNumber(temperatureField, text, TEXT_SIZE_LARGE, PHASE_TAG_NONE, false,
                                  STEAM_ZONE_X, STEAM_ZONE_Y, STEAM_ZONE_WIDTH, STEAM_ZONE_HEIGHT,
                                  STEAM_X, STEAM_BASELINE, moved, true);
  if (moved) {
    putText(FONT_UNIT, xEnd + TEMPERATURE_DEGREE_DX, STEAM_DEGREE_Y, COLOR_TEXT, F("4"));
    putText(FONT_UNIT, xEnd + TEMPERATURE_UNIT_DX, STEAM_UNIT_BASELINE, COLOR_TEXT, F("0"));
  }
}

// "ERR" in white, in 30 px capitals: FontErrorSmall ('0' = E, '1' = R)
void drawTemperatureError() {
  tft.fillRect(TEMPERATURE_ZONE_X, TEMPERATURE_ZONE_Y, TEMPERATURE_ZONE_WIDTH, TEMPERATURE_ZONE_HEIGHT, COLOR_BG);
  temperatureField.valid = false;   // the temperature has to be drawn again in full
  putText(FONT_ERROR_SMALL, TEMPERATURE_X, TEMPERATURE_ERROR_BASELINE, COLOR_TEXT, F("011"));
}

// Phase tag at a given position. FontTag only holds the letters P R E W A I T F L U S H, under the
// codes '0'..';' (see Fonts.h): "012" is PRE, "3456" is WAIT and "789:;" is FLUSH.
static void drawTag(PhaseTag tag, int16_t x, int16_t baseline) {
  if (tag == PHASE_TAG_LIMIT) {
    putText(FONT_TAG, x, baseline, COLOR_TAG, F("85<56"));   // FontTag codes: L I M I T
    return;
  }
  // FontTag codes: P R E = "012", C A L = "=48", F L U S H = "789:;", W A I T = "3456"
  putText(FONT_TAG, x, baseline, COLOR_TAG,
          tag == PHASE_TAG_PREINFUSION ? F("012") :
          (tag == PHASE_TAG_CALIBRATION ? F("=48") : (tag == PHASE_TAG_FLUSH ? F("789:;") : F("3456"))));
}

// Volume in ml, with its tag (PRE, CAL...) when there is one. The tag is redrawn only if redrawTag.
void drawVolume(uint16_t value, TextSize size, PhaseTag tag, bool redrawTag, uint8_t progress) {
  // The tag already drawn is kept when only the number changes: the zone of the number stops where the
  // tag starts.
  const bool keepTag = (tag != PHASE_TAG_NONE) && !redrawTag;
  int16_t zoneWidth = VOLUME_ZONE_WIDTH;
  if (keepTag) {
    zoneWidth = VOLUME_TAG_X - VOLUME_ZONE_X;
  }

  char text[6];
  itoa(value, text, 10);

  bool moved;
  const int16_t xNumberEnd = drawNumber(volumeField, text, size, tag, tag != PHASE_TAG_NONE && redrawTag,
                                        VOLUME_ZONE_X, VOLUME_ZONE_Y, zoneWidth, VOLUME_ZONE_HEIGHT,
                                        VOLUME_X, VOLUME_BASELINE, moved, false, progress);

  if (moved) {
    // The "ml" (small or large digits) can reach under the tag, in a part of the field that the zone
    // above does not cover: erased too, below the tag.
    if (keepTag) {
      tft.fillRect(VOLUME_TAG_X, VOLUME_TAG_BASELINE + 1,
                   VOLUME_ZONE_X + VOLUME_ZONE_WIDTH - VOLUME_TAG_X,
                   VOLUME_ZONE_Y + VOLUME_ZONE_HEIGHT - (VOLUME_TAG_BASELINE + 1), COLOR_BG);
    }

    putText(FONT_UNIT, xNumberEnd + VOLUME_UNIT_DX, VOLUME_UNIT_BASELINE, COLOR_TEXT, F("21"));
  }

  if (tag != PHASE_TAG_NONE && redrawTag) {
    drawTag(tag, VOLUME_TAG_X, VOLUME_TAG_BASELINE);
  }
}

// The flow limit: tenths of ml/s, "2:5 ml/s" (the decimal point is the glyph ':' of FontSmall, as in drawTemperature), in the volume
// field with the tag LIMIT. The tag is drawn only if redrawTag; the number is drawn again only when it changed (drawNumber).
void drawFlowLimit(uint16_t tenths, bool redrawTag) {
  char text[6];
  itoa(tenths / 10, text, 10);
  const uint8_t length = strlen(text);
  text[length] = ':';
  text[length + 1] = (char)('0' + (tenths % 10));
  text[length + 2] = 0;

  const bool keepTag = !redrawTag;
  int16_t zoneWidth = VOLUME_ZONE_WIDTH;
  if (keepTag) {
    zoneWidth = VOLUME_TAG_X - VOLUME_ZONE_X;
  }
  bool moved;
  const int16_t xNumberEnd = drawNumber(volumeField, text, TEXT_SIZE_SMALL, PHASE_TAG_LIMIT, redrawTag,
                                        VOLUME_ZONE_X, VOLUME_ZONE_Y, zoneWidth, VOLUME_ZONE_HEIGHT,
                                        VOLUME_X, VOLUME_BASELINE, moved);
  if (moved) {
    if (keepTag) {
      tft.fillRect(VOLUME_TAG_X, VOLUME_TAG_BASELINE + 1,
                   VOLUME_ZONE_X + VOLUME_ZONE_WIDTH - VOLUME_TAG_X,
                   VOLUME_ZONE_Y + VOLUME_ZONE_HEIGHT - (VOLUME_TAG_BASELINE + 1), COLOR_BG);
    }
    putText(FONT_UNIT, xNumberEnd + VOLUME_UNIT_DX, VOLUME_UNIT_BASELINE, COLOR_TEXT, F("2163"));   // m l / s
  }
  if (redrawTag) {
    drawTag(PHASE_TAG_LIMIT, VOLUME_TAG_X, VOLUME_TAG_BASELINE);
  }
}

// Duration in seconds, with its tag (WAIT, FLUSH) when there is one.
void drawDuration(uint16_t value, TextSize size, PhaseTag tag, bool redrawTag, uint8_t progress) {
  // The tag already drawn is kept when only the number changes: the zone of the number stops where the
  // tag starts.
  const bool keepTag = (tag != PHASE_TAG_NONE) && !redrawTag;
  int16_t zoneWidth = DURATION_ZONE_WIDTH;
  if (keepTag) {
    zoneWidth = DURATION_TAG_X - DURATION_ZONE_X;
  }

  char text[6];
  itoa(value, text, 10);

  bool moved;
  const int16_t xNumberEnd = drawNumber(durationField, text, size, tag, tag != PHASE_TAG_NONE && redrawTag,
                                        DURATION_ZONE_X, DURATION_ZONE_Y, zoneWidth, DURATION_ZONE_HEIGHT,
                                        DURATION_X, DURATION_BASELINE, moved, false, progress);

  if (moved) {
    // The "s" (or the old one) can be under the tag, in a part of the field that the zone above does not cover: with large digits, and with
    // small ones too (the "s" of a number of two digits reaches x 169, the tag starts at x 163)
    if (keepTag) {
      tft.fillRect(DURATION_TAG_X, DURATION_TAG_BASELINE + 1,
                   DURATION_ZONE_X + DURATION_ZONE_WIDTH - DURATION_TAG_X,
                   DURATION_ZONE_Y + DURATION_ZONE_HEIGHT - (DURATION_TAG_BASELINE + 1), COLOR_BG);
    }

    putText(FONT_UNIT, xNumberEnd + DURATION_UNIT_DX, DURATION_UNIT_BASELINE, COLOR_TEXT, F("3"));
  }

  if (tag != PHASE_TAG_NONE && redrawTag) {
    drawTag(tag, DURATION_TAG_X, DURATION_TAG_BASELINE);
  }
}

// Only the tag (used for the blinking of FLUSH).
void drawPhaseTag(PhaseTag tag, bool visible) {
  if (visible) {
    drawTag(tag, DURATION_TAG_X, DURATION_TAG_BASELINE);
  } else {
    // Only the rows of the letters: the number and its unit are not touched
    tft.fillRect(DURATION_TAG_X, DURATION_TAG_BASELINE - DURATION_TAG_HEIGHT,
                 DURATION_ZONE_X + DURATION_ZONE_WIDTH - DURATION_TAG_X, DURATION_TAG_HEIGHT, COLOR_BG);
  }
}

// The small dot next to the temperature that shows the heater is on (it blinks with the heater). steaming: the place of the steaming screen.
void drawHeaterIndicator(bool visible, bool steaming) {
  // Glyph '5' of FontUnit, centered on the cursor: drawn in the background color it erases the dot
  putText(FONT_UNIT, steaming ? STEAM_DOT_X : TEMPERATURE_DOT_X, steaming ? STEAM_DOT_Y : TEMPERATURE_DOT_Y,
          visible ? COLOR_TEXT : COLOR_BG, F("5"));
}
