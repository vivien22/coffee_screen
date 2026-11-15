/*
  config.h: every pin, limit, timing and tuning constant of the machine, in one place.
  (The flow sensor inputs A0/A1 and its calibration stay in USN_FlowSensor.h: they are
  tied to the PCINT1 implementation of that library.)
*/

#pragma once
#include <Arduino.h>

// ---------- Pins ----------
#define PIN_ENCODER_GA    2     // interrupt pins: D2 and D3 on Nano
#define PIN_ENCODER_GB    3
#define PIN_TFT_CS        10
#define PIN_TFT_DC        7
#define PIN_TFT_RST       8
#define PIN_TFT_BL        9     // backlight
#define PIN_HEATER_RELAY  5
#define PIN_PUMP_RELAY    4
#define PIN_MAX31865_CS   6

// ---------- CJMCU-111 rotary encoder ----------
#define ENCODER_STEP      1     // value change per detent
#define LONG_PUSH_MS      800   // a push held this long (ms) is a long push (reported before the release)
#define DOUBLE_PUSH_MS    350   // max delay (ms) between two short pushes for a double push (0 = disabled)
                                // a short push is reported once this delay has passed

// ---------- Parameters ----------
#define VALUE_MIN            0
#define TEMPERATURE_MAX      199
#define VOLUME_MAX           999
#define DURATION_MAX         999     // also the software limit of an extraction (s)
#define DEFAULT_TEMPERATURE  93      // used when the EEPROM has never been written
#define DEFAULT_VOLUME       40

// ---------- Temperature sensor (MAX31865 + PT100) ----------
#define RREF                       428.0
#define RNOMINAL                   100.0
#define VALID_TEMPERATURE_MIN      0
#define VALID_TEMPERATURE_MAX      199
#define RTD_THREE_WIRE             false   // true for a 3-wire PT100 (2-wire and 4-wire: false)
#define MAX31865_SPI_HZ            1000000UL
#define SENSOR_PERIOD_MS           250UL
#define TEMP_FILTER_TIME_CONSTANT  2.0     // low-pass filter time constant (s)

// ---------- Heater: PID + feedforward ----------
#define PID_WINDOW_MS        1000
// The PID is activated only if the temperature is at least PID_WORKING_RANGE from the setpoint
#define PID_WORKING_RANGE    20
#define PID_KP               40.0
#define PID_KI               0.1
#define PID_KD               500.0

// While water leaves the boiler, the same amount of cold water enters it. Heating that
// water from WATER_INLET_TEMPERATURE to the setpoint takes
//     power [W] = flow [ml/s] x 4.186 J/(g.K) x (setpoint - inlet temperature) [K]    (1 ml = 1 g)
// The heater is switched on for the matching part of the PID window as soon as the water
// flows, BEFORE the sensor sees the temperature drop. The PID only corrects what the
// feedforward does not cover.
#define HEATER_POWER_W           1100.0f   // power of the heating element (W): set it to your boiler
#define WATER_INLET_TEMPERATURE  22.0f     // temperature of the water entering the boiler (degrees C)
#define WATER_SPECIFIC_HEAT      4.186f    // J/(g.K)
#define FEEDFORWARD_GAIN         0.3f      // 0 = feedforward off, 1 = full estimate; tune on a real extraction

// ---------- Extraction ----------
#define VOLUME_PERIOD_MS         250UL     // volume check / end of extraction
#define MAX_PRESS_TIME_AGE_MS    5000UL    // older push timestamps are ignored (timer starts "now")

// ---------- Screen ----------
// Draw the three icons at startup. Disabled: the icon data (about 2.1 KB) is then not in the flash.
#define DISPLAY_ICONS

// ---------- Screen refresh ----------
#define TEMPERATURE_DISPLAY_MS   1000UL    // max refresh rate of the live temperature
#define SAVE_BLINK_MS            500UL     // the value disappears this long when it is saved
