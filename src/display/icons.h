// Display data for the GC9A01 coffee screen (240 x 240): colors, POSITIONS, icons
// Mockup: Interface_circulaire_minimaliste_92_C_38_ml_48s.png (1186 px circle -> 240 px screen)
//
//   1. Colors (background, text, icons)
//   2. Positions of icons and texts (everything is here: the sketch contains no coordinates)
//   3. Icons: 2-bit opacity masks, compressed (icons_data.h), drawn with COLOR_ICON
#pragma once
#include <Arduino.h>

// ============================== 1. Colors ==============================
#define COLOR_BG                      0x1040 // mockup background (21,8,1)
#define COLOR_TEXT                    0xFFBC // cream (254,245,228)
#define COLOR_ICON                    0xFDCD // icon orange (255,186,107); edges are blended with COLOR_BG
#define COLOR_TAG                     COLOR_ICON // phase tag (PRE, WAIT): same orange as the icons

// ========================== 2. Positions ============================
// Icons: X, Y = top-left corner
#define ICON_THERMOMETER_X            53
#define ICON_THERMOMETER_Y            35
#define ICON_DROP_X                   38
#define ICON_DROP_Y                   99
// Mode letter in the drop (A = AUTO, M = MANUAL): 15 px capital in the tag font and color, centered in the drop
#define MODE_LETTER_DX                10    // from ICON_DROP_X to the cursor of the letter
#define MODE_LETTER_DY                36    // from ICON_DROP_Y to its baseline
#define ICON_CLOCK_X                  56   // ink x 56 -> 94: compromise between the original place (x 65) and the axis of the thermometer (x 46)
#define ICON_CLOCK_Y                  167

// For texts, Y is the baseline of the GFX font (_BASELINE).
// Zones (_ZONE_) are erased with COLOR_BG before each value update.

// ---------- Temperature: "92 °C" (left-aligned, up to 3 digits) ----------
// The row is shifted to the left so that 199 stays inside the round screen.
#define TEMPERATURE_X                 87    // left edge of the number
#define TEMPERATURE_BASELINE          71    // baseline of the number
#define TEMPERATURE_UNIT_BASELINE     68    // baseline of the "C"
#define TEMPERATURE_ERROR_BASELINE    64    // baseline of "ERR" (sensor fault): 30 px capitals, centered on the thermometer
#define TEMPERATURE_DEGREE_Y          53    // center of the degree sign
#define TEMPERATURE_ZONE_X            86    // zone erased before each update: x 86 -> 220
#define TEMPERATURE_ZONE_Y            22    // y 22 -> 74
#define TEMPERATURE_ZONE_WIDTH        134
#define TEMPERATURE_ZONE_HEIGHT       52
#define TEMPERATURE_DEGREE_DX         6     // offset of the degree center from the end of the number
#define TEMPERATURE_UNIT_DX           11    // offset of the "C" from the end of the number

// --------------------- Blinking dot for PID output ---------------------
#define TEMPERATURE_DOT_X       80
#define TEMPERATURE_DOT_Y       39

// ---------- Volume: "38 ml" (left-aligned, up to 3 digits) ----------
#define VOLUME_X                      80    // left edge of the number
#define VOLUME_BASELINE               145   // baseline of the number
#define VOLUME_UNIT_BASELINE          144   // baseline of "ml"
#define VOLUME_ZONE_X                 79    // zone erased before each update: x 79 -> 240 (3 digits max, the drop is not touched)
#define VOLUME_ZONE_Y                 96    // y 96 -> 148
#define VOLUME_ZONE_WIDTH             161
#define VOLUME_ZONE_HEIGHT            52
#define VOLUME_UNIT_DX                5     // offset of "ml" from the end of the number
// "PRE" tag (pre-infusion volume): top right of the volume field, above the "ml". Fixed position,
// small or large digits (up to "50", pre-infusion maximum).
#define VOLUME_TAG_X                  164   // left edge of the tag
#define VOLUME_TAG_BASELINE           113   // baseline of the tag

// ---------- Duration: "48 s" (left-aligned, up to 3 digits) ----------
#define DURATION_X                    (ICON_CLOCK_X + 43)   // left edge of the number
#define DURATION_BASELINE             201   // baseline of the number
#define DURATION_UNIT_BASELINE        201   // baseline of the "s"
#define DURATION_ZONE_X               (DURATION_X - 1)   // zone erased before each update: x 98 -> 240 (the clock, up to x 94, is not touched)
#define DURATION_ZONE_Y               152   // y 152 -> 204
#define DURATION_ZONE_WIDTH           (240 - DURATION_ZONE_X)
#define DURATION_ZONE_HEIGHT          52
#define DURATION_UNIT_DX              1     // offset of the "s" from the end of the number
// Phase tag ("PRE", "WAIT", "FLUSH"): top right of the field, above the "s". Its position is fixed,
// small or large digits (countdown, or setting being edited): it does not move when the number
// changes width, and it clears "10s" in both sizes (a tag next to the small "10s" would overlap it,
// and "FLUSH" would not fit on the round screen there).
#define DURATION_TAG_X                (DURATION_X + 64)   // left edge of the tag
#define DURATION_TAG_BASELINE         169                 // baseline of the tag
#define DURATION_TAG_HEIGHT           15                  // height of the letters of the tag (erased when the tag blinks)

// ============================ 3. Icons (compressed 2-bit opacity) ============================
// 0 = background, 1 = 1/3 of the icon color, 2 = 2/3, 3 = full color. The data (size of each icon, then a bit
// stream of runs, read by drawIcon() in display.cpp) is generated from tools/icons_raw.h by
// ./tools/compress_icons.sh: edit the icons there, not here.
#include "icons_data.h"
