# coffee_screen, branch `pid-tuning`

Firmware to **tune the boiler PID and its feedforward** from the serial port, on the Arduino Nano of the
Ascaso Steel Uno Pro controller. It is `master` without the screen, the encoder, the settings and the
extraction phases: only what regulates the temperature is left, with a serial console to change every tuning
value while the machine runs, and a CSV telemetry to see what happens.

The values you find are copied back into `src/Config.h` (on this branch, then on `master`).

> [!WARNING]
> Not a substitute for an independent thermal fuse / thermostat. The heater is switched off above 110 °C on
> the probe (`VALID_TEMPERATURE_MAX`) and the setpoint cannot go above 105 °C (`PROBE_SETPOINT_MAX`), but a
> relay stuck on is not something the software can handle. Stay near the machine while you tune.

## What is different from `master`

- Everything is the temperature of the **probe** (screwed on the boiler). No `TEMPERATURE_OFFSET` here:
  the setpoint is the probe setpoint.
- The heater starts **off** (`DEFAULT_SETPOINT 0`): nothing heats until you send `set <°C>`.
- The feedforward can be **delayed**: it uses the flow measured `delay` seconds ago (0 to 19.75 s). The
  probe sees the cold water late, so the heating that answers it does not have to start at the same time.
- Nothing is saved in the EEPROM: after a reset the values of `Config.h` are back.
- The pump is started by a command (`pump <s>`), with its valve, for a given time.

## Serial console

115200 baud, one command per line. Names are not case sensitive. Every answer starts with `OK` or `ERR`;
lines starting with `#` are comments.

| Command | Effect |
|---|---|
| `set <°C>` | probe setpoint, 0 = heater off, limited to 105 |
| `kp <v>`, `ki <v>`, `kd <v>` | PID gains (the other two are kept) |
| `ff <gain>` | feedforward gain: 0 = off, 1 = the full estimate of the energy needed by the water entering |
| `delay <s>` | feedforward delay in seconds (steps of 0.25 s) |
| `power <W>` | power of the heating element, used by the feedforward |
| `inlet <°C>` | temperature of the water entering the boiler, used by the feedforward |
| `heater <0\|1>` | 0: the heater stays off whatever the controller computes; 1: normal |
| `valve <0|1>` | the valve alone: 1 opens it, 0 closes it (until the next `valve` or `stop`; `pump` opens it too) |
| `pump <s>` | pump + valve on for `s` seconds (at most `PUMP_MAX_SECONDS`), `pump 0` stops it |
| `stream <ms>` | period of the telemetry lines, 0 = off |
| `status` | the current values, named as in `Config.h` (to copy them there), and one telemetry line |
| `stop` | pump and valve off, setpoint 0 (heater off) |
| `help` | the list |

The pump also stops by itself when the In flow sensor does not count `NO_FLOW_MIN_ML` every
`NO_FLOW_TIMEOUT_MS` (empty tank, pump or sensor problem).

## Telemetry

One line every `TELEMETRY_PERIOD_MS` (1 s, change it with `stream`), after a header line:

    ms,probe,setpoint,pid_ms,ff_ms,on_ms,heater,pump,flow_in,vol_in,vol_out

| Column | Meaning |
|---|---|
| `ms` | time since the start |
| `probe` | filtered probe temperature, °C (`nan` if the sensor is not valid: the heater is then off) |
| `setpoint` | probe setpoint, °C |
| `pid_ms`, `ff_ms`, `on_ms` | milliseconds of heating in the current 1 s window: from the PID, from the feedforward, total (`pid_ms` can be negative: the PID takes from the feedforward) |
| `heater` | state of the heater relay |
| `pump` | 1 while the pump runs |
| `flow_in` | water entering the boiler, ml/s |
| `vol_in`, `vol_out` | ml counted by the In and Out sensors since the pump started |

To keep a log and look at it:

    pio device monitor --filter log2file        # or any terminal that saves the output
    python3 tools/plot_log.py <logfile> [picture.png]    # needs matplotlib

## Suggested way to tune

1. **PID alone.** `ff 0`, `set 98` (the probe setpoint), `stream 250`. Watch the rise, the overshoot and the
   steady state with the pump off. Adjust `kp`, then `ki` (slowly: the window is 1 s and the boiler is slow),
   then `kd`. Full power is used until the probe is within `PID_WORKING_RANGE` (20 °C) of the setpoint.
2. **The disturbance.** With a stable probe, `pump 20` and note how far and how late the probe drops.
3. **Feedforward.** Raise `ff` (try 0.3, then 0.5...) until the drop gets smaller. Then look at *when* the heating
   starts: if the probe first rises and then drops, the heating is too early: increase `delay`. If the drop is
   already there when the heating comes, decrease it. `power` and `inlet` are the physics of the estimate; tune
   `ff` first.
4. `status`, copy the values into `Config.h`.

## Hardware

Same pins as `master` (`Config.h`): heater relay D5, pump relay D4, valve relay A2, MAX31865 chip select D6,
flow sensors A0 (In) and A1 (Out, ignored: `FLOW_OUT_SENSOR` is commented out, Out = 0). The screen and the encoder are not used. Without the flow sensors, uncomment
`#define FLOW_SIMULATOR` in `Config.h` (commented out by default, the real In sensor is wired): the pump relay then makes simulated water flow (3 ml/s) and the
feedforward can be seen working. Leave it commented out to use the real sensors.

The tuning only needs the **In** sensor (feedforward, no-flow safety). While the Out sensor is not connected,
comment out `#define FLOW_OUT_SENSOR` in `Config.h`: A1 is then ignored and `vol_out` stays 0.

Build with PlatformIO, environment `nanoatmega328new` (the Adafruit libraries of `master` are no longer
needed, only `br3ttb/PID`).

## Tests

    ./test/run.sh

Host tests (no Arduino): heater controller (window, limits, feedforward and its delay), serial commands, pump
safeties and telemetry, temperature sensor. They run on GitHub Actions with cppcheck.
