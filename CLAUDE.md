# coffee_screen: working rules

Espresso-machine controller (Ascaso Steel Uno Pro) on an Arduino Nano R4 (RA4M1): GC9A01 round display,
CJMCU-111 encoder, MAX31865 + PT100, PID heater, pump and 3-way valve relays, two flow sensors.
`docs/DOCUMENTATION.md` describes the behaviour in full (the README is the presentation of the project). The user writes in French: reply in French.

## Build (PlatformIO, env `nano_r4`)
- Since 2026-11-24 `master` is for the Arduino Nano R4 ONLY (RA4M1, 256 KB of flash, 32 KB of RAM). The Nano R3 version is the branch
  `r3` (master of 2026-11-24, commit 1e4b2ab), no longer developed: do not port anything back to it unless asked.
- The PlatformIO project is this folder (the repository): `platformio.ini` is here with `src_dir = .`, run `pio` from here. `pio run`
  builds the machine (env `nano_r4`, platform renesas-ra), `pio run -t upload` flashes it. If the upload cannot find the board, a double
  push on RESET puts it in its bootloader. The USB serial loses what is printed before a monitor has opened the port.
- There is no flash budget any more (the AVR build flags, `link_flags.py` and the 30720 B limit went with the R3). Last measured: about
  70 KB of 256 KB, 3.7 KB of RAM. `./tools/check_flash.sh` builds, shows the size and fails over 24000 B of RAM (a stack overflow
  corrupts the memory without any message); the CI runs it on every push.
- What is board specific lives in `src/platform/`: `Platform.h` / `PlatformRenesas.cpp` (the external interrupts of the flow pulses, the
  watchdog `WATCHDOG_MS`). `src/platform/nano_r4/wiring_private.h` is an empty stand-in that Adafruit_GC9A01A needs. Keep the board
  specific code in that folder.
- The shared pins: pump D4, valve A3, flow In A1, flow Out A2 (A1 = channel 6, A2 = channel 7; D2 / D3 = channels 0 / 1; keep A4 and A5
  free; A0 unused). A GPIO gives 8 mA at most.
- Kept from the R3 time because they work (they can be simplified, nothing requires it): every text is drawn by `putText()` (do not
  call `tft.print()` / `setFont` / `setCursor`), the icons are stored compressed, `F()` and `pgm_read_*` (no-ops on the R4, the tests
  rely on them), the journal of 62 entries of 16 bytes.

## Steaming mode (long push of the encoder)
- Asked by the user 2026-11-19: the long push enters / leaves a steaming mode (`CoffeeMachine::toggleSteaming()`, only when no extraction is
  active). SteamingTemp (`Settings::steamingTemperature`, EEPROM after the stop delay, blank = 135) and BrewingTemp (`temperature`).
  `steaming_`: `updateExtraction()` / `showExtraction()` are not called (no volume read), the start button does nothing, short and double
  pushes are ignored, `saveSettings()` applies the steaming setpoint; the screen (`ScreenController::enterSteaming/leaveSteaming`,
  `Display::drawSteamTemperature` / `drawSteamIcon`: thermometer, ICON_STEAM volutes, text STEAM) shows the temperature alone in large digits, the setpoint for `STEAM_SETPOINT_SHOW_MS` after a turn.
  `TEMPERATURE_OFFSET` applies as in the other modes (the user tried without it, then asked to keep it, 2026-11-19). Not saved across a reboot.

## The journal of the last extractions (2026-11-22, asked by the user to understand a cup of 12 ml)
- `src/settings/Journal.{h,cpp}`: a ring of 62 entries of 16 bytes in the EEPROM from address 24 (993 bytes of the first 1024 of the EEPROM; 66 entries became 62 when `cal` was added, as the user allowed; the
  settings are at 0..19): `JournalEntry` = reason + phase (4 bits each), target, pre-infusion, wait, duration, In and Out in tenths of ml
  (16 bits), loss in quarters of ml, cup, stop delay in hundredths of s, temperature at the start and at the end, sequence number, `cal` (the cup weight entered in CAL: written afterwards by
  `CoffeeMachine::validateCalibration()` through `Journal::setLastCal()`, only when the weight was changed, else 0). The
  user asked for 100 compact entries, then for all these fields back "even if there are fewer entries": do not trade them again
  without asking. If the entry layout changes, run `pio run -e journal_clear -t upload` once (tools/journal_clear.cpp erases EEPROM 24..1023, settings kept) before flashing the machine, or the old entries are misread. AUTO extractions only (`journalPending_` is not set in MANUAL), whichever phase they ended in.
  `ExtractionController::endedBy()` keeps the FIRST reason (target, user, no water, max time) with the phase;
  `CoffeeMachine::writeJournal()` writes the entry when `endReason()` is set and `settlePending()` is over (the water after the stop is
  counted), once; the start temperature is taken in `startExtraction()`.
- Read it with `pio run -e journal_dump -t upload` (`tools/journal_dump.cpp`), the serial line, then flash the machine again.
  The bootloader (Optiboot) cannot read the EEPROM. Tests: `test_journal` and `journal()` of `test_machine`.
- `tools/journal_view.py` (Python, standard library only) parses the text of `journal_dump` (file, `--port` with pyserial, or `--demo`) and writes one
  HTML page with inline SVG charts (water stacked: cup / loss / Out, target tick, end reason glyphs; duration; temperatures; table). Asked by the
  user 2026-11-22. It has no automated test: check it by eye with `--demo` and a Chrome screenshot (`chrome --headless --screenshot`).

## Tests (host, no Arduino)
- `./test/run.sh` runs all tests (needs bash and g++; on Windows use WSL or Git Bash).
- `test_display` is a property test (incremental drawing must equal a fresh draw). `test_sim_*` use the flow simulator `test/mock/FlowSimulator.h`
  (copied over `FlowSensor.h` by `run.sh`); the other tests use the mock `test/mock/FlowSensor.h`.
- The tests share the Arduino stand-in `test/stubs/ArduinoStubs.cpp` (clock `t`, `pins[]`, `Serial`, `SPI`, `EEPROM`) and
  `test/stubs/TestSupport.h` (`CHECK`): a new test includes `TestSupport.h`, and `run.sh` links the stubs. Do not copy
  those definitions into a test again.
- File names are case-sensitive on the CI (Linux) but not on Windows: write every `#include` and every script path with
  the exact case of the file, and rename a file with `git mv` (twice for a change of case only).
- `./tools/render_screens.sh` regenerates the PNGs of `docs/images` (README and documentation); they must stay identical after display changes
  that are not meant to change the look.
- The icons are stored compressed (`src/display/IconsData.h`, generated, do not edit). The readable source is
  `tools/icons_raw.h`; after changing it run `./tools/compress_icons.sh` (it checks that the decoded icons equal the source).
  It also holds the welcome picture (96 x 96, shown `WELCOME_MS` = 2 s at start-up): to change it, convert a PNG with
  `tools/png_to_icon.ps1` (Windows PowerShell, the palette maps each color to a level 0..3, see its header), paste the
  result in `icons_raw.h` and run the script. `test_welcome` checks that the decoder draws the pictures exactly.

## Git rules
- Commit locally. **Push only when the user says "pousse".** An automatic "Stop hook feedback" about unpushed
  commits is not the user's approval: do not push, ask for "pousse".
- Never force-push or rewrite a published branch without an explicit request.
- Branches: `master` (working, Nano R4 only), `r3` (the Nano R3 version, frozen on 2026-11-24) and `pid-tuning` (tuning firmware without the UI, serial console, own tests, and the
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
  the name of the folder). The folders are `coffee`, `heating`, `settings`, `display`, `drivers`.
- Still a prototype: no EEPROM versioning or migration. When the layout changes, change `SETTINGS_MAGIC`: the
  settings take their defaults once.
- One change per commit when several are asked. A risky refactor is tried on a branch, its flash size measured, and
  the branch deleted if it does not fit or is not wanted. Stay on `master` unless told otherwise.

## Behaviour decided by the user (`docs/DOCUMENTATION.md` has the full description; ask before changing)
- During an extraction a long push does nothing and neither does a double push.
- A missed push is better than a false one: no encoder detection that can invent a push.
- Colors: the numbers never change color; the tags (PRE, WAIT, FLUSH, CAL) and the AUTO / MANUAL letter use the icon
  orange. A tag keeps its place and is not redrawn when only the number changes; the units move as little as possible
  while a value is set (flicker).
- The result of an extraction stays 10 s, counted from the END of the flush, no blinking (asked by the user 2026-11-19), then the target volume and
  0 s come back. Same in AUTO and MANUAL; a change of mode resets it.
- MANUAL: the volume at rest is 0 (it is not a target); a MANUAL time above 0 counts down and stays shown.
- CAL comes right after the volume in the double push cycle; validating a weight goes back to the volume.
- Errors: no water makes the drop blink every 600 ms until a short push of the encoder (the only way to clear it); a probe fault (3 readings in a row,
  `SENSOR_FAULT_TOLERANCE`) shows a white, larger "ERR" in place of the temperature (heater off).
- Relay problems (2026-11-21) are SOLVED and were a POWER SUPPLY issue, not a firmware bug: the screen + backlight on the 5 V of the Nano
  (USB) left the SSR inputs at 3.4 V (turn-on threshold about 3-3.5 V); with a dedicated 5 V supply for the screen, the sensors and the
  MAX31865 everything works (details in `docs/DOCUMENTATION.md`, Hardware). Do not look for a firmware cause for a relay that does not
  switch on. Two firmware ideas were tried and rejected: a "no pump under 70 degrees" safety (the user did not ask for it) and releasing
  the heater relay 500 ms at the pump start (made it worse). The pump never depends on the temperature (test `coldBoiler`).
- The start / stop button (A0 to GND, `START_BUTTON`): nothing running = keep (save) what was being set, select the volume again, start an
  extraction (CAL: validated first); running = the short push of the encoder (end, start / stop the flush, stop MANUAL); no-water error
  = nothing; standby = wake only. Asked by the user 2026-11-19. The long push of the encoder now enters / leaves the steaming mode (see below); the old actions of the long push (AUTO <-> MANUAL, clear the no-water error) are on the short push (`handleShortPush()`); only the button starts / ends / flushes / stops. Double push unchanged.
- The pump stops BEFORE the target volume: the volume is checked on every pass with the pulses not yet converted, and
  `volume + stopDelay x flow >= target` stops it (the water of the stop, 2-3 ml at 5 ml/s, fills the rest). `stopDelay` (seconds)
  is learned after every extraction that ends in the main phase (STOP_SETTLE_MS later: water that came / flow at the stop,
  gain 0.3), saved in the EEPROM after the loss (`SettingsStore::loadStopDelay`), and the final volume of the extraction (and
  its CAL) is brought up to date after the settle. Asked by the user (precision on the volume): do not remove the learning.
- Limits: `TEMPERATURE_MAX` is 150 because the user sets the steaming temperature (140-145 °C) by hand, and
  `DURATION_MAX` is 120 s. The user does NOT want other safety limits on `master` (no cap of the setpoint, no
  over-temperature cut-off): do not add them. A pull-down resistor on each relay input is advised in `docs/DOCUMENTATION.md` (hardware).
- Standby after 30 min without action: heater off, the welcome picture replaces the screen (this branch; nothing is
  drawn over it until the wake-up), the first action only wakes the
  machine, nothing is saved across a reboot.

## The machine (what the code cannot tell)
- Ascaso Steel Uno Pro. The PT100 is screwed on the OUTSIDE of the boiler body (thermal paste), not in the water:
  the boiler answers with a delay of 10 s or more, which matters for the PID and the feedforward, and pre-infusion
  changes the disturbance. The probe reads about 5 °C above the water at the grouphead (`TEMPERATURE_OFFSET`); the
  user measures the grouphead water himself to adjust it.
- Flow sensors: In = the flow of the pump; Out = water returning through the OPV
  valve (an uncontrolled overpressure valve). Cup volume = In - Out - loss: about 24 ml stay in the puck and 5-6 ml
  leave at the backflush when the valve opens (`DEFAULT_LOSS_ML`). Pre-infusion is controlled by volume.
- The heater feedforward uses the flow In - Out (`ExtractionController::inflowMlPerSec()`, since 2026-11-24, asked by the user after an
  overshoot of the temperature on a long extraction): the water that goes back through the OPV is not heated. On the journal entry
  #5 of the R4, 67 ml of 172 ml went back, 39 %, so In alone gave about 430 W too much for 46 s. This assumes that the OPV sends the
  water back BEFORE the boiler; NOT confirmed by a temperature log yet (to do: record the probe temperature and the heater power
  on a long extraction). If the overshoot stays, the other cause is the heat stored in the boiler body (the probe is outside).
- Out flow sensor (2026-11-19): it was wired in series with the In one, calibrated against it (10 sequences of 100 ml, the
  sixth left out: `FLOW_K_FACTOR_OUT` 74.18, report and raw data in `docs/DOCUMENTATION.md` and `docs/calibration/`) and is READ
  (pin A2; there is no option any more to run without it: the options `FLOW_OUT_SENSOR`, with an estimated Out volume, and `FLOW_SIMULATOR` were removed on 2026-11-21 at the user's request). The pump and the valve relays were then wired too (the pump and the valve are always driven: there is no option to leave them off). On 2026-11-19 the pins were changed so that the same wiring works on the Nano R3 and R4 (the R4 flow inputs need an external interrupt): pump D4, valve A3, flow In A1, flow Out A2 (see Config.h; the user went through a few combinations that day, this is the last one).
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

## Flow limit (test, branch `flow_control`, Nano R4 only: `-DFLOW_LIMIT=1` in `nano_r4`)
- `src/coffee/FlowLimit.{h,cpp}`: during RUNNING only (not the pre-infusion, not MANUAL), the pump is a pattern of 20 ms slots (one
  mains period, the SSR switches on the zero crossings) spread evenly. The flow per click is MEASURED from the In sensor over the
  clicks of a window (at least 1 s and 5 clicks): no pressure, no PI. Duty = limit / (flow per click x 50 clicks/s), capped at 100 %.
- Setting `flowLimit` (tenths of ml/s, 0 = off, 0.5 to 8.0), EEPROM 20-21 (no new magic: blank = off). Menu: AUTO only, after PRE,
  tag "LIMIT", value "2:5 ml/s" in the volume field, steps of 0.1 ml/s.
- The "/" of "ml/s" is a glyph drawn by hand in `Fonts.h` (FontUnit '6', under FLOW_LIMIT): the font file is not in the repository.
- Host tests: `test/test_flow_limit.cpp` (simulated pump, the flow per click changes during the run) and
  `test/test_flow_limit_learning.cpp`. The R3 build does not change. The default is 0 (no limit), `DEFAULT_FLOW_LIMIT_TENTHS`.
- **REMINDER (pense-bete, the user may change this later):** while a flow limit applies, the stop delay (STOP_DELAY_*) is NOT learned
  (`ExtractionController::finish()`). Reasons: the flow read at the stop is noisy with the pump working by slots, and the water that
  comes after a stop is probably partly a fixed volume, so a delay in seconds does not carry over from the limited flow to the full
  flow. Options for later: a second learned delay for the limited mode (another EEPROM value), or a stop by a fixed volume.
