# coffee_screen: working rules

Espresso-machine controller (Ascaso Steel Uno Pro) on an Arduino Nano (ATmega328P): GC9A01 round display,
CJMCU-111 encoder, MAX31865 + PT100, PID heater, pump and 3-way valve relays, two flow sensors.
The README describes the behaviour. The user writes in French: reply in French.

## Build (PlatformIO, env `nanoatmega328new`)
- The PlatformIO project is this folder (the repository): `platformio.ini` is here with `src_dir = .`, run `pio` from here.
- `pio run` compiles; the line "Flash: ... used N bytes from 30720 bytes" is what matters (limit 30720 B, RAM 2 KB).
- **`build_flags` in `platformio.ini` must keep `-mcall-prologues`** (about 1 KB saved) **and `-fno-ipa-cp -mstrict-X
  -fno-gcse -fshort-enums`** (1262 B saved together, measured one by one then combined). `-flto`, `-fno-jump-tables`,
  `-ffunction-sections`, `-Wl,--gc-sections` brought nothing (PlatformIO already does LTO and gc-sections): do not add
  them. `-finline-limit=3 -fno-reorder-blocks` would give 154 B more: left out, they slow the inlined loops.
- Flash is tight: report the size after every change that touches the display, the machine or the settings.
- The limit stays 30720 B. Tested on the user's Nano (2026-11-17): the bootloader is the 512 B Optiboot at 0x7E00, but the
  fuses protect the whole 2 KB boot section: a write above 0x7800 fails (verification error at 0x7800) and leaves a
  broken firmware. Using up to 32256 B would need an ISP programmer on the ICSP header to set BOOTSZ (hfuse 0xDE, like an
  Uno). Never raise `maximum_size` in platformio.ini without that.
- Last measured (flags above, welcome picture and standby): 30054 B flash, 470 B RAM as configured (the pump and the valve
  driven, the In flow sensor only); 30040 B with the valve operated by hand (no `DRIVE_VALVE`), 29626 B boiler only (no
  `DRIVE_PUMP`).
- `./tools/check_flash.sh` builds, shows the flash size and fails over 1600 B of RAM (a stack overflow corrupts the
  memory without any message); the CI runs it for the three flow configurations (In only, In and Out, simulator) on every push. There is no flash budget any
  more (the user's decision, the software being nearly finished: the linker already refuses a program over 30720 B);
  `FLASH_BUDGET=<bytes>` gives an optional gate. The simulator is off by default: the real flow sensors build is
  about 170 B larger than the simulator one. With the welcome picture (1048 B) and its use in the standby (about 190 B) the firmware is 30054 B of
  30720 B with the pump and the valve driven,
  and the feedforward table of the PID tuning (+220 B) will still fit (about 450 B left). With `WELCOME_MS` 0 the firmware
  is 40 B larger than master (the faster drawing of the icons and the start-up sequence), with the same RAM.

## Tests (host, no Arduino)
- `./test/run.sh` runs all tests (needs bash and g++; on Windows use WSL or Git Bash).
- `test_display` is a property test (incremental drawing must equal a fresh draw). `test_boiler_only` runs with `DRIVE_PUMP` and `DRIVE_VALVE`
  switched off in a copy of Config.h (the boiler only) and `test_valve_by_hand` with `DRIVE_VALVE` switched off (the pump
  works, the valve output stays off); the other tests use the Config.h as it is.
- The tests share the Arduino stand-in `test/stubs/ArduinoStubs.cpp` (clock `t`, `pins[]`, `Serial`, `SPI`, `EEPROM`) and
  `test/stubs/TestSupport.h` (`CHECK`): a new test includes `TestSupport.h`, and `run.sh` links the stubs. Do not copy
  those definitions into a test again.
- File names are case-sensitive on the CI (Linux) but not on Windows: write every `#include` and every script path with
  the exact case of the file, and rename a file with `git mv` (twice for a change of case only).
- `./tools/render_screens.sh` regenerates the README PNGs; they must stay identical after display changes
  that are not meant to change the look.
- The icons are stored compressed (`src/display/IconsData.h`, generated, do not edit). The readable source is
  `tools/icons_raw.h`; after changing it run `./tools/compress_icons.sh` (it checks that the decoded icons equal the source).
  It also holds the welcome picture (96 x 96, shown `WELCOME_MS` = 5 s at start-up): to change it, convert a PNG with
  `tools/png_to_icon.ps1` (Windows PowerShell, the palette maps each color to a level 0..3, see its header), paste the
  result in `icons_raw.h` and run the script. `test_welcome` checks that the decoder draws the pictures exactly.

## Git rules
- Commit locally. **Push only when the user says "pousse".** An automatic "Stop hook feedback" about unpushed
  commits is not the user's approval: do not push, ask for "pousse".
- Never force-push or rewrite a published branch without an explicit request.
- Branches: `master` (working) and `pid-tuning` (tuning firmware without the UI, serial console, own tests, and the
  measured tuning results in `tuning/`). The other branches were merged into master and deleted.
- End commit messages with: `Co-Authored-By: Claude <noreply@anthropic.com>`

## Display
- Custom bit-packed Adafruit GFX fonts (`src/display/Fonts.h`), already reduced to the glyphs in use.
- Digits are drawn as opaque glyph blocks with fixed-width cells so a number changes without a blank moment.
- Start-up sequence: `ScreenController::begin()` shows the welcome picture, the probe and the flow sensors start while it is
  shown, `finishWelcome()` waits for the rest of `WELCOME_MS`, clears the picture and draws the icons in the dark, then
  `update()` draws the values and `show()` lights the screen. Measured on the board: 311 ms of black screen between the
  picture and the real screen, 193 ms of which are the first values (the drawing of the digit blocks is the slow part).
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
- Standby after 30 min without action: heater off, the welcome picture replaces the screen (this branch; nothing is
  drawn over it until the wake-up), the first action only wakes the
  machine, nothing is saved across a reboot.

## The machine (what the code cannot tell)
- Ascaso Steel Uno Pro. The PT100 is screwed on the OUTSIDE of the boiler body (thermal paste), not in the water:
  the boiler answers with a delay of 10 s or more, which matters for the PID and the feedforward, and pre-infusion
  changes the disturbance. The probe reads about 5 °C above the water at the grouphead (`TEMPERATURE_OFFSET`); the
  user measures the grouphead water himself to adjust it.
- Flow sensors: In = water entering the boiler (used by the feedforward); Out = water returning through the OPV
  valve (an uncontrolled overpressure valve). Cup volume = In - Out - loss: about 24 ml stay in the puck and 5-6 ml
  leave at the backflush when the valve opens (`DEFAULT_LOSS_ML`). Pre-infusion is controlled by volume.
- Wiring on 2026-11-17 (later that day): everything is connected on the board (probe, encoder, screen, relays, the
  In flow sensor) except the Out flow sensor. So `FLOW_SIMULATOR` and `FLOW_OUT_SENSOR` are both commented out in
  Config.h: the real In sensor is read and A2 (the Out input) is ignored and the volume out is estimated: Out = In * `FLOW_OUT_ESTIMATE_RATIO` (0.385, from the user: a 50 ml cup goes with 50 ml returned by the OPV, with the 30 ml loss; the CAL calibration absorbs the rest).
  Uncomment `FLOW_OUT_SENSOR` when the Out sensor is wired (`pid-tuning` has the same option). The pump and the valve relays were then wired too: `DRIVE_PUMP` and `DRIVE_VALVE` are both on. For a test the user
  swapped the two GPIOs and operated the valve by hand for a while (pump A2 / valve D4, `DRIVE_VALVE` off), then went back to
  the normal wiring (pump D4, valve A2). On 2026-11-19 the pins were changed so that the same wiring works on the Nano R3 and R4 (the R4 flow inputs need an external interrupt): valve A0, flow In A1, flow Out A2 (see Config.h). Without `DRIVE_VALVE` the valve output stays LOW and the extraction runs with the
  pump only; without `DRIVE_PUMP` only the boiler is controlled.
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
