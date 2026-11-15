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
#define PIN_VALVE_RELAY   A2
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
// Pre-infusion: volume (ml) and wait time (seconds). 0 skips the matching step, see "Extraction" below.
#define PREINFUSION_MAX_ML                50
#define DEFAULT_PREINFUSION_ML            10
#define PREINFUSION_WAIT_MAX_SECONDS      10
#define DEFAULT_PREINFUSION_WAIT_SECONDS  5
#define FLUSH_MAX_SECONDS                 10
#define DEFAULT_FLUSH_SECONDS             5

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

// The water that enters the boiler (inflow sensor) is cold. Heating that
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
#define VOLUME_PERIOD_MS             250UL     // volume check / end of extraction

// Flow simulator: replaces the two flow sensors when they are not connected. While the pump is on,
// the simulated water enters the boiler at FLOW_SIM_PUMP_ML_PER_S and FLOW_SIM_OPV_RATIO of it
// comes back through the OPV valve. Comment this line out to use the real sensors (A0 / A1).
#define FLOW_SIMULATOR
#define FLOW_SIM_PUMP_ML_PER_S       3.0f      // water entering the boiler while the pump is on (ml/s)
#define FLOW_SIM_OPV_RATIO           0.1f      // part of it returning through the OPV (0 = none, 0.1 = 10 %)
#define FLOW_SIM_UPDATE_MS           100UL     // period of the simulated measurement

// The target volume is the volume wanted in the cup. The flow sensors measure the water that
// enters the boiler (In) and the water that comes back through the OPV valve (Out); the water
// that really stays in the system is In - Out. Not all of it ends in the cup:
//   PUCK_RETAINED_ML  water that stays in the puck (depends on the dose and on the coffee)
//   BACKFLUSH_ML      water sent to the drip tray when the valve opens at the end of the extraction
// Volume shown on the screen = (In - Out) - PUCK_RETAINED_ML - BACKFLUSH_ML, never below 0.
// The extraction ends when it reaches the target volume, i.e. when In - Out reaches
// target + PUCK_RETAINED_ML + BACKFLUSH_ML. Tune both values by weighing the cups.
#define PUCK_RETAINED_ML             24.0f
#define BACKFLUSH_ML                 6.0f
#define EXTRACTION_RESULT_DISPLAY_MS 10000UL   // an extraction without flush: its volume and duration stay on the screen
                                               // this long, then the screen shows the target volume and 0 s again

// Pre-infusion: the puck is wetted at low pressure, then left to rest, before the full extraction.
//   PREINFUSION       pump ON  + valve open   (until the pre-infusion VOLUME has entered the boiler)
//   PREINFUSION_WAIT  pump OFF + valve open   (wait TIME, set on the screen)
//   RUNNING           pump ON  + valve open   (until the target volume is reached)
// The pre-infusion volume (0 to PREINFUSION_MAX_ML) and the wait time (0 to PREINFUSION_WAIT_MAX_SECONDS)
// are settings saved in EEPROM:
//   pre-infusion volume = 0: no pre-infusion at all (the wait is skipped too)
//   wait time = 0:           only the wait is skipped
// The pre-infusion volume is the water counted by the flow sensors (In - Out), not the volume
// in the cup. Safety: the pre-infusion ends after PREINFUSION_TIMEOUT_MS even if the volume is not
// reached (flow sensor or pump problem).
#define PREINFUSION_TIMEOUT_MS       30000UL
//
// Flush: after the extraction, the group head is rinsed with hot water.
//   FLUSH_WAIT        pump OFF + valve closed (waits for a short push)
//   FLUSHING          pump ON  + valve open   (flush time, set on the screen)
// The flush time is a setting (0 to FLUSH_MAX_SECONDS, saved in EEPROM):
//   flush time = 0:  no flush at all (FLUSH_WAIT is skipped too, the extraction ends in FINISHED)

#define MAX_PRESS_TIME_AGE_MS        5000UL    // older push timestamps are ignored (timer starts "now")

// ---------- Screen ----------
// Draw the three icons at startup. Disabled: the icon data (about 1 KB) is then not in the flash.
#define DISPLAY_ICONS

// ---------- Screen refresh ----------
#define TEMPERATURE_DISPLAY_MS   1000UL    // max refresh rate of the live temperature
#define SAVE_BLINK_MS            500UL     // the value disappears this long when it is saved
#define FLUSH_BLINK_MS           600UL     // the "FLUSH" tag is alternately shown and hidden this long during FLUSHING
