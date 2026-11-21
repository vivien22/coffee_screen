/*
  Config.h: every pin, limit, timing and tuning constant of the machine, in one place.
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
#define PIN_VALVE_RELAY   A3
#define PIN_MAX31865_CS   6
#define PIN_START_BUTTON  A0    // the start / stop button of the extraction (see START_BUTTON below)
// Flow sensors: they need an interrupt. Nano R3: any of A0..A5 (PCINT1, see USN_FlowSensor). Nano R4: A1 is external
// interrupt channel 6 and A2 channel 7 (A0 has none; D2 / D3, the encoder, are channels 0 / 1; keep A4 free: it shares
// channel 1 with D3, and A5: it shares channel 2 with A3, the valve relay, a plain output). The same wiring works on both
// boards. A0 is not used.
#define PIN_FLOW_IN       A1
#define PIN_FLOW_OUT      A2

// ---------- CJMCU-111 rotary encoder ----------
#define ENCODER_STEP      1     // value change per detent
#define LONG_PUSH_MS      800   // a push held this long (ms) is a long push (reported before the release):
                                // it enters and leaves the steaming mode
#define DOUBLE_PUSH_MS    350   // max delay (ms) between two short pushes for a double push (0 = disabled)
                                // a short push is reported once this delay has passed

// ---------- Start / stop button ----------
// START_BUTTON: a push button between PIN_START_BUTTON and GND (the internal pull-up is used: no resistor) that only starts and
// stops the extraction. Nothing running: whatever was being set is kept (saved), the volume is selected again and an extraction
// starts. During an extraction: it ends it (then it starts and stops the flush, or stops MANUAL).
// Ignored after a no-water error; in standby the first push only wakes the machine. Comment it out if there is no button.
#define START_BUTTON
#define START_BUTTON_DEBOUNCE_MS  30UL   // the level has to stay this long (ms) to count

// ---------- Parameters (limits and defaults of the settings) ----------
#define VALUE_MIN            0
#define TEMPERATURE_MAX      150     // up to the steaming temperature (140-145 degrees C): set by hand when needed
// The digits of a value have a fixed width (see Display.cpp): the volume has 2 digits at most, and the values
// shown next to a tag in the duration field (WAIT, FLUSH) 1 digit, so that nothing leaves the round screen or
// overlaps the tag.
#define VOLUME_MAX           99
#define DURATION_MAX         120     // the software limit of an extraction (s): it stops with an error after that
#define DEFAULT_TEMPERATURE  93      // used when the EEPROM has never been written
#define DEFAULT_VOLUME       40
// Steaming mode (long push of the encoder): the heater only regulates the steaming temperature (SteamingTemp in the EEPROM,
// the temperature of the brewing is BrewingTemp), no extraction, the screen shows the temperature alone. The temperature
// is set with the encoder (shown STEAM_SETPOINT_SHOW_MS after each turn, then the measured one is shown again).
#define DEFAULT_STEAMING_TEMPERATURE  135    // used when the EEPROM has never been written
#define STEAM_SETPOINT_SHOW_MS        2000UL
// Pre-infusion: volume (ml) and wait time (seconds). 0 skips the matching step (see docs/DOCUMENTATION.md, "Extraction phases").
#define PREINFUSION_MAX_ML                50
#define DEFAULT_PREINFUSION_ML            10
#define PREINFUSION_WAIT_MAX_SECONDS      9
#define DEFAULT_PREINFUSION_WAIT_SECONDS  5
#define FLUSH_MAX_SECONDS                 9
#define DEFAULT_FLUSH_SECONDS             5

// Mode: AUTO (volume, pre-infusion, flush) or MANUAL (the pump runs between two pushes); a short push
// switches it (the long push is the steaming mode). AUTO when the EEPROM has never been written.
// MANUAL time (seconds, 0 = until the second push): setting of the MANUAL mode.
//
// The settings are saved in EEPROM by the machine, never by the user, and only at these moments (and
// only the bytes that changed are written, so the EEPROM wears only when a value really changes):
//   - a double push (the next parameter is selected: the one that was edited is validated),
//   - the start of an extraction (the volume that was edited is validated),
//   - a change of mode (short push).
// A temperature setpoint is applied to the heater at the same moments.
#define MANUAL_MAX_SECONDS                99
#define DEFAULT_MANUAL_SECONDS            0

// ---------- Temperature offset and standby ----------
// On this coffee machine (Ascaso Steel Uno Pro) the temperature of the boiler probe is higher than the temperature
// of the water at the grouphead. The setpoint and the temperature shown are the temperature of the
// water at the grouphead: the heater regulates the probe at setpoint + TEMPERATURE_OFFSET and the screen shows
// probe - TEMPERATURE_OFFSET (degrees C). Set it to 0 if your probe reads the same as the water at the grouphead.
#define TEMPERATURE_OFFSET   5

// Standby: after STANDBY_MINUTES without any action (encoder turn or push, extraction) the heater is switched
// off and the welcome picture replaces the screen (nothing changes on the screen if WELCOME_MS is 0). Any action wakes
// the machine up (that first action is ignored) and the screen comes back. Nothing is saved:
// after a reboot the machine is awake. 0 = no standby.
#define STANDBY_MINUTES      30

// ---------- Temperature sensor (MAX31865 + PT100) ----------
#define RREF                       428.0
#define RNOMINAL                   100.0
#define VALID_TEMPERATURE_MIN      0
#define VALID_TEMPERATURE_MAX      199
#define RTD_THREE_WIRE             false   // true for a 3-wire PT100 (2-wire and 4-wire: false)
#define MAX31865_SPI_HZ            1000000UL
#define SENSOR_PERIOD_MS           250UL
#define SENSOR_FAULT_TOLERANCE     3       // faulty readings in a row (fault flag, implausible value) before "ERR" and heater off
#define TEMP_FILTER_TIME_CONSTANT  2.0     // low-pass filter time constant (s)

// ---------- Heater: PID + feedforward ----------
#define PID_WINDOW_MS        1000
// When the pump starts while the heater relay is on (a cold boiler: full power, the relay stays on), the heater relay is released
// for this long (ms): the relays of the pump and of the valve have to pull in on a supply that already feeds the screen and the
// heater relay (seen on the bench: the pump relay did not pull in under 70 degrees shown, where the heater is at full power).
// 0 = never.
#define HEATER_PAUSE_AT_PUMP_START_MS  500
// The PID is activated only if the temperature is at least PID_WORKING_RANGE from the setpoint
#define PID_WORKING_RANGE    20
#define PID_KP               40.0
#define PID_KI               0.3   // measured on the boiler (tuning/README.md on pid-tuning): no permanent offset
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
// 1.0 = the full estimate, measured on the real boiler (2026-11-17, pid-tuning branch, 20 s of pump = 100 ml): the probe drop
// goes from 11.5 C (0) to 9.7 (0.3), 8.7 (0.5), 8.0 (0.7), 7.75 (1.0). The element (1100 W) is already at full power from the
// start of the flow, so a delay of the feedforward only makes it worse: none here.
#define FEEDFORWARD_GAIN         1.0f      // 0 = feedforward off, 1 = full estimate

// ---------- Flow sensors ----------
// Calibration, in pulses per millilitre. In: water entering the boiler. Out: return of the OPV valve.
// Out was calibrated against In on 2026-11-19 with the two sensors in series (10 sequences of 100 ml, sequence 6 left out:
// 74.18 pulses per ml, see docs/DOCUMENTATION.md, "Calibration of the flow sensors").
// In: 25 runs (pulses against the volume measured at the output), docs/calibration/in_flow_sensor.csv.
#define FLOW_K_FACTOR_IN             12.56f
#define FLOW_K_FACTOR_OUT            74.18f
#define FLOW_UPDATE_MS               250UL     // period of the volume / flow rate computation

// ---------- Pump and valve ----------
// DRIVE_PUMP: the pump relay is driven and extractions can be started. Commented out (only the boiler is connected, or
// being tested): the pump output stays LOW (off) and no extraction can be started (a push does nothing when the machine
// is idle); the heater is controlled as usual.
#define DRIVE_PUMP
// DRIVE_VALVE: the 3-way valve relay is driven by the phases of the extraction. Commented out (the valve is operated by
// hand): the valve output stays LOW and the extraction runs as usual with the pump only.
#define DRIVE_VALVE

// ---------- Extraction ----------
#define VOLUME_PERIOD_MS             250UL     // volume check / end of extraction

// The Out sensor (A2, return of the OPV valve) is connected and calibrated (FLOW_K_FACTOR_OUT): it is read. Comment this line
// out if it is not connected: the pin A2 is then left alone (a floating input would count false pulses) and the volume out
// is ESTIMATED from the volume in: Out = In * FLOW_OUT_ESTIMATE_RATIO. The user's figure: a 50 ml cup goes with 50 ml
// returned by the OPV, so with the default loss of 30 ml In = 50 + 50 + 30 = 130 ml and the ratio is 50 / 130 = 0.385.
// The CAL calibration absorbs the rest (it corrects the loss).
#define FLOW_OUT_SENSOR
#define FLOW_OUT_ESTIMATE_RATIO      0.385f    // used only without FLOW_OUT_SENSOR

// Flow simulator: uncomment this line to replace the two flow sensors when they are not connected. While the pump
// is on, the simulated water enters the boiler at FLOW_SIM_PUMP_ML_PER_S and FLOW_SIM_OPV_RATIO of it comes back
// through the OPV valve. Commented out: the real sensors are read (A1 for In, A2 for Out if FLOW_OUT_SENSOR).
// #define FLOW_SIMULATOR
#define FLOW_SIM_PUMP_ML_PER_S       3.0f      // water entering the boiler while the pump is on (ml/s)
#define FLOW_SIM_OPV_RATIO           0.1f      // part of it returning through the OPV (0 = none, 0.1 = 10 %)

// The target volume is the volume wanted in the cup. The flow sensors measure the water that
// enters the boiler (In) and the water that comes back through the OPV valve (Out); the water
// that really stays in the system is In - Out. Not all of it ends in the cup: the puck retains some
// (it depends on the dose and on the coffee) and some is sent to the drip tray when the valve opens
// at the end of the extraction (backflush). The sum is the "loss":
//   volume shown on the screen = (In - Out) - loss, never below 0
// The extraction ends when it reaches the target volume, i.e. when In - Out reaches target + loss.
// The loss is a setting (saved in EEPROM), corrected by the machine itself: after an AUTO extraction
// the CAL parameter appears in the cycle of the double push, the real weight of the cup is entered
// there (1 g = 1 ml), and the double push that validates it corrects the loss:
//     measured loss = (In - Out) - real volume
//     new loss = loss + CALIBRATION_GAIN * (measured loss - loss)        (kept between 0 and LOSS_MAX_ML)
// An extraction that gave less than CALIBRATION_MIN_ML (stopped at once...) is not offered.
#define DEFAULT_LOSS_ML              30.0f     // used when the EEPROM has never been written
#define LOSS_MAX_ML                  60.0f
#define CALIBRATION_GAIN             0.5f      // 1 = a weighing replaces the loss, 0.5 = the mean of both
#define CALIBRATION_MIN_ML           10

// Stop before the target. When the pump stops the water goes on for a moment (the pulses not yet counted, the water in
// the pipes and the pump slowing down): measured on 2026-11-19, 100 ml of In were followed by 2 to 3 ml more. The pump is
// stopped when the volume counted so far PLUS what is still to come reaches the target; what is still to come is the
// flow at that moment times a delay, STOP_DELAY_* seconds. The delay is LEARNED: after every extraction that ends in the main
// phase the water that came after the stop (STOP_SETTLE_MS later) is compared with what was expected, and the delay moves
// STOP_DELAY_GAIN of the way toward the one that would have been right. It is saved (EEPROM) and used by the next extraction.
#define STOP_DELAY_DEFAULT_S         0.40f     // first value (2 ml at 5 ml/s), used while nothing was learned
#define STOP_DELAY_MAX_S             1.5f      // the delay is kept between 0 and this
#define STOP_DELAY_GAIN              0.30f     // 1 = the last extraction decides alone, 0 = no learning
#define STOP_SETTLE_MS               2000UL    // how long after the stop the water that still comes is measured
#define STOP_LEARN_MIN_ML_PER_S      2.0f      // no learning from an extraction stopped under this flow (too noisy)
#define STOP_ANTICIPATION_MAX_ML     6.0f      // the pump never stops more than this before the target
#define EXTRACTION_RESULT_DISPLAY_MS 10000UL   // the volume and duration of an extraction (end of the flush if any) stay on the screen
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

// MANUAL mode: a short push starts the pump (valve open), the next short push stops it. With a MANUAL
// time above 0 the pump also stops by itself after that time, and the screen counts it down to 0
// instead of showing the elapsed time. No target volume, no pre-infusion, no flush. The screen shows the water that stays in the system (In - Out,
// without the loss) and the elapsed time; they stay
// EXTRACTION_RESULT_DISPLAY_MS at the end, like an extraction without flush. The no-flow safety and
// the DURATION_MAX limit are active.

// No-flow safety: while the pump runs (PREINFUSION and RUNNING), the water that enters the boiler
// (In sensor alone) must increase by at least NO_FLOW_MIN_ML every NO_FLOW_TIMEOUT_MS. Otherwise
// the pump is stopped and the extraction ends in ERROR_NO_WATER (empty tank, pump or In sensor
// problem). Out is not used: with a choked puck the water comes back through the OPV (In - Out
// stops), but the tank is not empty. Not used during the flush.
// Keep NO_FLOW_MIN_ML well below the real flow x NO_FLOW_TIMEOUT_MS.
#define NO_FLOW_TIMEOUT_MS           5000UL
#define NO_FLOW_MIN_ML               2.0f
// After a no-flow error the drop icon blinks (DROP_BLINK_MS) until a short push of the encoder resets the error;
// a short or double push does nothing meanwhile.
//
// Flush: after the extraction, the grouphead is rinsed with hot water.
//   FLUSH_WAIT        pump OFF + valve closed (waits for a short push)
//   FLUSHING          pump ON  + valve open   (flush time, set on the screen)
// The flush time is a setting (0 to FLUSH_MAX_SECONDS, saved in EEPROM):
//   flush time = 0:  no flush at all (FLUSH_WAIT is skipped too, the extraction ends in FINISHED)

#define MAX_PRESS_TIME_AGE_MS        5000UL    // older push timestamps are ignored (timer starts "now")

// ---------- Watchdog ----------
// If loop() does not run for WATCHDOG_TIMEOUT, the ATmega328P resets itself: the pump, the valve
// and the heater relays are released (pins back to inputs) until the setup runs again.
// Comment the line out to disable it. Choose WDTO_4S or WDTO_8S: a Nano with the OLD bootloader
// does not disable the watchdog after a reset and can stay in a reset loop with a short timeout
// (flash the Optiboot bootloader, or select "ATmega328P (Old Bootloader)" only if you do not use this).
#define WATCHDOG_TIMEOUT   WDTO_4S   // Nano R3 (AVR constant)
#define WATCHDOG_MS        4000UL    // Nano R4: the same timeout in ms (its watchdog allows up to about 5.5 s)

// ---------- Screen ----------
// Draw the three icons at startup. Disabled: the icon data (about 1 KB) is then not in the flash.
#define DISPLAY_ICONS

// The welcome picture (a cup of coffee) stays on the screen this long at start-up (ms); 0 = none, and its data
// (about 1.2 KB of flash, its use in the standby included) is then not in the flash. Needs DISPLAY_ICONS.
#define WELCOME_MS               5000UL

// ---------- Screen refresh ----------
#define TEMPERATURE_DISPLAY_MS   1000UL    // max refresh rate of the live temperature
#define FLUSH_BLINK_MS           600UL     // the "FLUSH" tag is alternately shown and hidden this long during FLUSHING
#define DROP_BLINK_MS            600UL     // the drop icon is alternately shown and hidden this long after a no-flow error
