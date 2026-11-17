/*
  Config.h: pins, limits and starting values of the tuning firmware (branch pid-tuning).
  Everything marked "runtime" can be changed from the serial console while the machine runs
  (see the README); copy the values you keep back here.
*/

#pragma once
#include <Arduino.h>

// Number to text, for the help of the console
#define STRINGIFY_(x) #x
#define STRINGIFY(x)  STRINGIFY_(x)

// ---------- Pins ----------
#define PIN_HEATER_RELAY  5
#define PIN_PUMP_RELAY    4
#define PIN_VALVE_RELAY   A2
#define PIN_MAX31865_CS   6
#define PIN_FLOW_IN       A0    // flow sensors: both on A0..A5 (PCINT1), see USN_FlowSensor
#define PIN_FLOW_OUT      A1

// ---------- Serial console ----------
#define SERIAL_BAUD            115200
#define TELEMETRY_PERIOD_MS    1000UL   // runtime ("stream"): one CSV line every this many ms, 0 = off

// ---------- Temperature sensor (MAX31865 + PT100) ----------
#define RREF                       428.0
#define RNOMINAL                   100.0
#define VALID_TEMPERATURE_MIN      0
// Safety: above this probe temperature the reading is declared invalid and the heater is switched off
#define VALID_TEMPERATURE_MAX      110
#define RTD_THREE_WIRE             false   // true for a 3-wire PT100 (2-wire and 4-wire: false)
#define MAX31865_SPI_HZ            1000000UL
#define SENSOR_PERIOD_MS           250UL
#define SENSOR_FAULT_TOLERANCE     3       // faulty readings in a row (fault flag, implausible value) before "ERR" and heater off
#define TEMP_FILTER_TIME_CONSTANT  2.0     // low-pass filter time constant (s)

// ---------- Heater: PID + feedforward ----------
// Everything here is the temperature of the PROBE (screwed on the boiler), not of the water at the grouphead.
#define PROBE_SETPOINT_MAX   105     // the setpoint cannot be set higher (VALID_TEMPERATURE_MAX cuts the heater at 110)
#define DEFAULT_SETPOINT     0       // probe setpoint at start-up, 0 = heater off until "set" is sent (runtime: set)

#define PID_WINDOW_MS        1000
// The PID is activated only if the probe is at least PID_WORKING_RANGE below the setpoint (full power before)
#define PID_WORKING_RANGE    20
#define PID_KP               40.0    // runtime: kp
#define PID_KI               0.1     // runtime: ki
#define PID_KD               500.0   // runtime: kd

// The water that enters the boiler (inflow sensor) is cold. Heating it from the inlet temperature to the
// setpoint takes
//     power [W] = flow [ml/s] x 4.186 J/(g.K) x (setpoint - inlet temperature) [K]    (1 ml = 1 g)
// The heater is switched on for the matching part of the PID window; the PID corrects what is not covered.
#define HEATER_POWER_W           1100.0f   // runtime: power
#define WATER_INLET_TEMPERATURE  22.0f     // runtime: inlet
#define WATER_SPECIFIC_HEAT      4.186f    // J/(g.K)
#define FEEDFORWARD_GAIN         0.3f      // runtime: ff.  0 = feedforward off, 1 = full estimate
// The probe reacts late: the feedforward uses the flow measured this long ago (runtime: delay, seconds).
// It is kept in steps of FEEDFORWARD_STEP_MS, up to (FEEDFORWARD_HISTORY - 1) steps (19.75 s).
#define FEEDFORWARD_DELAY_MS     0UL
#define FEEDFORWARD_STEP_MS      250UL
#define FEEDFORWARD_HISTORY      80

// ---------- Flow sensors ----------
// Comment this line out while the Out sensor is not connected: A1 is then ignored (no interrupt, Out volume 0).
// The tuning only needs the In sensor.
// #define FLOW_OUT_SENSOR

// Calibration, in pulses per millilitre. In: water entering the boiler. Out: return of the OPV valve.
#define FLOW_K_FACTOR_IN             12.56f
#define FLOW_K_FACTOR_OUT            50.0f
#define FLOW_UPDATE_MS               250UL     // period of the volume / flow rate computation

// Flow simulator: replaces the two flow sensors when they are not connected. While the pump is on,
// the simulated water enters the boiler at FLOW_SIM_PUMP_ML_PER_S and FLOW_SIM_OPV_RATIO of it
// comes back through the OPV valve. Comment this line out to use the real sensors (A0 / A1).
// #define FLOW_SIMULATOR
#define FLOW_SIM_PUMP_ML_PER_S       3.0f      // water entering the boiler while the pump is on (ml/s)
#define FLOW_SIM_OPV_RATIO           0.1f      // part of it returning through the OPV (0 = none, 0.1 = 10 %)

// ---------- Pump (command "pump <seconds>") ----------
// The pump and the valve are switched on together. Safeties: the pump stops by itself after the requested
// time (PUMP_MAX_SECONDS at most) and when the In sensor does not see NO_FLOW_MIN_ML every NO_FLOW_TIMEOUT_MS
// (empty tank, pump or sensor problem).
#define PUMP_MAX_SECONDS             120
#define NO_FLOW_TIMEOUT_MS           5000UL
#define NO_FLOW_MIN_ML               2.0f

// ---------- Watchdog ----------
// If loop() does not run for WATCHDOG_TIMEOUT, the ATmega328P resets itself: the pump, the valve
// and the heater relays are released (pins back to inputs) until the setup runs again.
// Comment the line out to disable it. Choose WDTO_4S or WDTO_8S (see the README of master about the old bootloader).
#define WATCHDOG_TIMEOUT   WDTO_4S
