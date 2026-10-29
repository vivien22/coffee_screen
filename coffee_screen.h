/*
  Declarations for coffee_screen.ino: pins of the encoder, settings, EEPROM addresses
  and the selected parameter.
  The screen (pins, types, drawing) is in display.h, positions and colors are in icons.h.
*/

#pragma once

#include <Arduino.h>

// ---------- Pins of the encoder ----------
#define PIN_ENCODER_GA   2     // interrupt pins: D2 and D3 on Nano
#define PIN_ENCODER_GB   3

// ---------- CJMCU-111 rotary encoder (adjusts the selected value) ----------
#define ENCODER_STEP     1     // value change per detent
#define TEMPERATURE_MAX  199   // largest value of each parameter
#define VOLUME_MAX       999
#define DURATION_MAX     999
#define LONG_PUSH_MS     800   // a push held this long (ms) is a long push (reported before the release)
#define DOUBLE_PUSH_MS   350   // max delay (ms) between two short pushes for a double push (0 = disabled)
                               // a short push is reported once this delay has passed

// ---------- Parameters EEPROM address ----------
// The values are stored one after the other, each one takes the size of an int
#define PARAMETERS_BASE_ADDR         ((int)0)
#define PARAMETERS_TEMPERATURE_ADDR  (PARAMETERS_BASE_ADDR)
#define PARAMETERS_VOLUME_ADDR       (PARAMETERS_TEMPERATURE_ADDR + sizeof(parameters.temperature.value))
#define PARAMETERS_DURATION_ADDR     (PARAMETERS_VOLUME_ADDR + sizeof(parameters.volume.value))

// ---------- Types ----------

// Parameter selected on the screen, adjusted by the encoder (a double push selects the next one)
enum ActiveParameter {
  PARAMETER_VOLUME,
  PARAMETER_DURATION,
  PARAMETER_TEMPERATURE
};
