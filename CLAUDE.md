# coffee_screen: working rules

Espresso-machine controller (Ascaso Steel Uno Pro) on an Arduino Nano (ATmega328P): GC9A01 round display,
CJMCU-111 encoder, MAX31865 + PT100, PID heater, pump and 3-way valve relays, two flow sensors.
The README describes the behaviour. The user writes in French: reply in French.
**This is the `pid-tuning` branch**: no display, encoder, settings or extraction phases (see its README). What CLAUDE.md says about
those (Display, the UI behaviours, icons, the pre-infusion limits) applies to `master`; the build, tests, Git and naming rules apply here too.

## Build (PlatformIO, env `nanoatmega328new`)
- The PlatformIO project is this folder (the repository): `platformio.ini` is here with `src_dir = .`, run `pio` from here.
- `pio run` compiles; the line "Flash: ... used N bytes from 30720 bytes" is what matters (limit 30720 B, RAM 2 KB).
- **`build_flags` in `platformio.ini` must keep `-mcall-prologues`** (about 1 KB saved) **and `-fno-ipa-cp -mstrict-X
  -fno-gcse -fshort-enums`** (1262 B saved together, measured one by one then combined). `-flto`, `-fno-jump-tables`,
  `-ffunction-sections`, `-Wl,--gc-sections` brought nothing (PlatformIO already does LTO and gc-sections): do not add
  them. `-finline-limit=3 -fno-reorder-blocks` would give 154 B more: left out, they slow the inlined loops.
- Flash is tight: report the size after every change that touches the display, the machine or the settings.
- Last measured (flags above, compressed icons, probe fault tolerance): 28590 B flash, 457 B RAM with `FLOW_SIMULATOR`
  on (what Config.h has); 28782 B flash, 464 B RAM with the real flow sensors (`FLOW_SIMULATOR` commented out).
- `./tools/check_flash.sh` builds and fails over 29800 B of flash or 1600 B of RAM; the CI runs it for both flow
  configurations on every push. About 1 KB is kept free for the feedforward table of the PID tuning: do not raise
  the budget without asking.

## Tests (host, no Arduino)
- `./test/run.sh` runs all tests (needs bash and g++; on Windows use WSL or Git Bash).
- `test_display` is a property test (incremental drawing must equal a fresh draw).
- The tests share the Arduino stand-in `test/stubs/ArduinoStubs.cpp` (clock `t`, `pins[]`, `Serial`, `SPI`, `EEPROM`) and
  `test/stubs/TestSupport.h` (`CHECK`): a new test includes `TestSupport.h`, and `run.sh` links the stubs. Do not copy
  those definitions into a test again.
- File names are case-sensitive on the CI (Linux) but not on Windows: write every `#include` and every script path with
  the exact case of the file, and rename a file with `git mv` (twice for a change of case only).
- `./tools/render_screens.sh` regenerates the README PNGs; they must stay identical after display changes
  that are not meant to change the look.
- The icons are stored compressed (`src/display/IconsData.h`, generated, do not edit). The readable source is
  `tools/icons_raw.h`; after changing it run `./tools/compress_icons.sh` (it checks that the decoded icons equal the source).

## Git rules
- Commit locally. **Push only when the user says "pousse".** An automatic "Stop hook feedback" about unpushed
  commits is not the user's approval: do not push, ask for "pousse".
- Never force-push or rewrite a published branch without an explicit request.
- Branches: `master` (working), `pid-tuning` (tuning firmware without the UI, serial console, own tests),
  `preinfusion-time` (old), `phase-table` (merged into master).
- End commit messages with: `Co-Authored-By: Claude <noreply@anthropic.com>`

## Display
- Custom bit-packed Adafruit GFX fonts (`src/display/Fonts.h`), already reduced to the glyphs in use.
- Digits are drawn as opaque glyph blocks with fixed-width cells so a number changes without a blank moment.
- Limits that keep the layout valid: volume <= 99, pre-infusion <= 50, wait and flush <= 9 s, manual <= 99 s.

## Code conventions (asked by the user)
- Identifiers and comments in English. Comments are generous and say why; the code is airy (blank lines between
  steps) and easy for a human to read before it is short. The word is "grouphead".
- Naming: a source file in `src/` has the name of its class in CamelCase (`CoffeeMachine.cpp`), data members end with
  `_` (`pumpPin_`), tests, tools and scripts are snake_case. `coffee_screen.ino` keeps its name (the Arduino IDE wants
  the name of the folder). The folders are `machine`, `heating`, `settings`, `display`, `drivers`.
- Still a prototype: no EEPROM versioning or migration. When the layout changes, change `SETTINGS_MAGIC`: the
  settings take their defaults once.
- One change per commit when several are asked. A risky refactor is tried on a branch, its flash size measured, and
  the branch deleted if it does not fit or is not wanted. Stay on `master` unless told otherwise.

## Behaviour decided by the user (README has the full description; ask before changing)
- During an extraction a long push does nothing (except clearing the no-water error) and neither does a double push.
- A missed push is better than a false one: no encoder detection that can invent a push.
- Colors: the numbers never change color; the tags (PRE, WAIT, FLUSH, CAL) and the AUTO / MANUAL letter use the icon
  orange. A tag keeps its place and is not redrawn when only the number changes; the units move as little as possible
  while a value is set (flicker).
- The result of an extraction stays 10 s, counted from the END of the flush, blinks twice, then the target volume and
  0 s come back. Same in AUTO and MANUAL; a change of mode resets it.
- MANUAL: the volume at rest is 0 (it is not a target); a MANUAL time above 0 counts down and stays shown.
- CAL comes right after the volume in the double push cycle; validating a weight goes back to the volume.
- Errors: no water makes the drop blink every 600 ms until a long push; a probe fault (3 readings in a row,
  `SENSOR_FAULT_TOLERANCE`) shows a white, larger "ERR" in place of the temperature (heater off).
- Limits: `TEMPERATURE_MAX` is 150 because the user sets the steaming temperature (140-145 °C) by hand, and
  `DURATION_MAX` is 120 s. The user does NOT want other safety limits on `master` (no cap of the setpoint, no
  over-temperature cut-off): do not add them. A pull-down resistor on each relay input is advised in the README (hardware).
- Standby after 30 min without action: heater off, nothing changes on the screen, the first action only wakes the
  machine, nothing is saved across a reboot.

## The machine (what the code cannot tell)
- Ascaso Steel Uno Pro. The PT100 is screwed on the OUTSIDE of the boiler body (thermal paste), not in the water:
  the boiler answers with a delay of 10 s or more, which matters for the PID and the feedforward, and pre-infusion
  changes the disturbance. The probe reads about 5 °C above the water at the grouphead (`TEMPERATURE_OFFSET`); the
  user measures the grouphead water himself to adjust it.
- Flow sensors: In = water entering the boiler (used by the feedforward); Out = water returning through the OPV
  valve (an uncontrolled overpressure valve). Cup volume = In - Out - loss: about 24 ml stay in the puck and 5-6 ml
  leave at the backflush when the valve opens (`DEFAULT_LOSS_ML`). Pre-infusion is controlled by volume.
- Wiring on 2026-11-17: the In flow sensor is connected, the Out one is not yet (hence `FLOW_SIMULATOR`; on
  `pid-tuning` the `FLOW_OUT_SENSOR` line of Config.h has to stay commented out for the same reason). On 2026-11-16
  the PT100 was not connected yet.
- Above about 120-130 °C the water turns to steam: another system (why `pid-tuning` cuts the heater at 110 °C). No
  software safety replaces an independent thermal fuse. Typical water inlet 22-24 °C (32 °C in summer).

## `pid-tuning` branch
- Goal: tune Kp, Ki, Kd and the feedforward gain from serial telemetry (CSV, 115200 baud) plotted on the PC
  (`tools/plot_log.py`), without display, encoder, EEPROM settings or extraction phases. Nothing heats until
  `set <temperature>` is sent. Setpoint is the probe's (no offset), limited to 105 °C, heater cut above 110 °C.
- Run no heating or extraction test unless the user is present. Not rebased on `master` for now (the user cancelled it):
  do it only if asked.

## Tried and dropped
- Merging `drawVolume` and `drawDuration`: tried on a branch, deleted (did not fit: 30792 B at that time).
- Measuring the inlet water temperature at power-up (cold boiler = ambient): abandoned by the user.
- `-fno-jump-tables`, `-ffunction-sections`, `-Wl,--gc-sections`: no gain (see Build).
- The phase table (one table row per phase) was tried on a branch, saved about 400 B and was merged into `master`
  on 2026-11-17; the user found it harder to read at first.
