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
- There is no flash budget any more (the AVR build flags, `link_flags.py` and the 30720 B limit went with the R3). Last measured (2026-11-24, 16-level icons, flow curves): 92 KB of 256 KB (35.1 %),
  6.8 KB of RAM. `./tools/check_flash.sh` builds, shows the size and fails over 24000 B of RAM (a stack overflow
  corrupts the memory without any message); the CI runs it on every push.
- What is board specific lives in `src/platform/`: `Platform.h` / `PlatformRenesas.cpp` (the external interrupts of the flow pulses, the
  watchdog `WATCHDOG_MS`). `src/platform/nano_r4/wiring_private.h` is an empty stand-in that Adafruit_GC9A01A needs. Keep the board
  specific code in that folder.
- The shared pins: pump D4, valve A3, flow In A1, flow Out A2 (A1 = channel 6, A2 = channel 7; D2 / D3 = channels 0 / 1; keep A4 and A5
  free; A0 unused). A GPIO gives 8 mA at most.
- Kept from the R3 time because they work (they can be simplified, nothing requires it): every text is drawn by `putText()` (do not
  call `tft.print()` / `setFont` / `setCursor`), the icons are stored compressed, `F()` and `pgm_read_*` (no-ops on the R4, the tests
  rely on them), the journal of 50 entries of 16 bytes (since 2026-11-24, to make room for the flow curves).

## Steaming mode (long push of the encoder)
- Asked by the user 2026-11-19: the long push enters / leaves a steaming mode (`CoffeeMachine::toggleSteaming()`, only when no extraction is
  active). SteamingTemp (`Settings::steamingTemperature`, EEPROM after the stop delay, blank = 135) and BrewingTemp (`temperature`).
  `steaming_`: `updateExtraction()` / `showExtraction()` are not called (no volume read), the start button does nothing, short and double
  pushes are ignored, `saveSettings()` applies the steaming setpoint; the screen (`ScreenController::enterSteaming/leaveSteaming`,
  `Display::drawSteamTemperature` / `drawSteamIcon`: thermometer, ICON_STEAM volutes, text STEAM) shows the temperature alone in large digits, the setpoint for `STEAM_SETPOINT_SHOW_MS` after a turn.
  `TEMPERATURE_OFFSET` applies as in the other modes (the user tried without it, then asked to keep it, 2026-11-19). Not saved across a reboot.

## The journal of the last extractions (2026-11-22, asked by the user to understand a cup of 12 ml)
- `src/settings/Journal.{h,cpp}`: a ring of 50 entries of 16 bytes in the EEPROM from address 24 (25..824; 66 entries became 62 when `cal` was added and 50 on 2026-11-24 for the flow curves, as the user allowed; the
  settings are at 0..19): `JournalEntry` = reason + phase (4 bits each), target, pre-infusion, wait, duration, In and Out in tenths of ml
  (16 bits), loss in quarters of ml, cup, stop delay in hundredths of s, temperature at the start and at the end, sequence number, `cal` (the cup weight entered in CAL: written afterwards by
  `CoffeeMachine::validateCalibration()` through `Journal::setLastCal()`, only when the weight was changed, else 0). The
  user asked for 100 compact entries, then for all these fields back "even if there are fewer entries": do not trade them again
  without asking. If the entry layout changes, run `pio run -e journal_clear -t upload` once (tools/journal_clear.cpp erases EEPROM 24..8191, settings kept) before flashing the machine, or the old entries are misread. AUTO extractions only (`journalPending_` is not set in MANUAL), whichever phase they ended in.
  `ExtractionController::endedBy()` keeps the FIRST reason (target, user, no water, max time) with the phase;
  `CoffeeMachine::captureJournal()` makes the entry when `endReason()` is set and `settlePending()` is over (the water after the stop is
  counted), once, and `writeFlash()` writes it when the extraction (its flush) is over (`add()` = ONE block operation: entry + next slot); the start temperature is taken in `startExtraction()`.
- Read it WITHOUT flashing: the machine answers the letter `j` on its USB serial line (115200 baud) with `JOURNAL`, one line per
  extraction (the last first) and `END` (`src/drivers/SerialConsole.{h,cpp}`, the line is made by `src/settings/JournalFormat.cpp`,
  integers only, same format as before). The letter `p` prints one line of the performance trace (`src/drivers/Perf.{h,cpp}`: longest loop pass,
  longest `screen_.update()`, number and longest time of the data flash block operations, since the previous `p`) and resets it. It says nothing by itself and writes nothing. `tools/journal_viewer.html` (double-click `tools/read_journal.bat`: Chrome or Edge, Web Serial, sends `j`, closes the
  port, draws; `#test` = self test, `#demo` = example) or `tools/journal_view.py --port COM12` ask and draw. The old sketch `tools/journal_dump.cpp` is gone. Tests: `test_journal`, `test_serial_console` (the stub `Serial` of
  `test/stubs/Arduino.h` keeps `out` and gives `in`) and `journal()` of `test_machine`.
- Flow curves (branch journal-flow-curves, asked by the user 2026-11-24 to see the flow In / Out and the volume Out in time): `coffee/FlowRecorder.{h,cpp}` takes one
  sample every `CURVE_SAMPLE_MS` = 500 ms (the water through In and Out since the previous sample, tenths of ml, uint8) from the start of an AUTO extraction until
  `captureJournal()`; the journal keeps the last `CURVE_COUNT` = 20 curves of at most `CURVE_SAMPLES` = 180 samples (90 s, user choice) in slots of 363 bytes from EEPROM address 825, right after the entries (8 KB data flash of the R4; map in `Journal.h`: 24 next slot, 25..824 entries, 825..8084 the 20 curve slots, 8085..8191 free; since 2026-11-25 there is NO stored next slot: the curve of the entry with sequence n is in slot `n % CURVE_COUNT`, header = sequence, count, CRC-8 of sequence + count + samples, valid only if all three match, so old data, half written curves and the old layout are ignored without `journal_clear`; `CURVE_COUNT` 20 does not divide 256, so for the 20 extractions after each wrap of the sequence up to 4 of the oldest curves are overwritten early). `Journal::beginCurve / stepCurve / readCurve(sequence, ...)`: `stepCurve()` is ONE block operation of the data flash (the curve is 1 or 2: the block that does not hold the header first, the header block last), called by `CoffeeMachine::writeFlash()` one per pass and never during an extraction; `curvesFit()` keeps a smaller EEPROM safe). The sequence number links a curve to its entry. `j` prints `CURVE #n 500 ms N | in ... | out ...` lines after the entries (`formatCurveLine`),
  `journal_viewer.html` draws them (flow In / Out, volume Out, hover, smoothing); `journal_view.py` ignores them. Tests: `test_flow_recorder`, `test_journal`,
  `test_serial_console`, `journal()` of `test_machine` (the stub EEPROM is now 8 KB). After this layout change run `pio run -e journal_clear -t upload` once (it now erases 24..8191 block by block, settings kept) before flashing the machine; NOT needed for the layout of 2026-11-25 (checksum).
- `tools/journal_view.py` (Python, standard library only) parses the text of the console (file, `--port` with pyserial, or `--demo`) and writes one
  HTML page with inline SVG charts (water stacked: cup / loss / Out, target tick, end reason glyphs; duration; temperatures; table). Asked by the
  user 2026-11-22. It has no automated test: check it by eye with `--demo` and a Chrome screenshot (`chrome --headless --screenshot`).

## Writes to the data flash (2026-11-25, branch smooth-ui)
- The R4 "EEPROM" is the 8 KB data flash in blocks of 1 KB: every call that changes something erases and programs a whole block (the loop waits).
  `src/settings/Flash.{h,cpp}` `flashWrite()` writes several ranges with ONE block operation per block touched (none if nothing changed), counted
  and timed in `drivers/Perf` (console command `p`). The stub EEPROM (`test/stubs/EEPROM.h`) counts `blockOps` like the board. Do not add
  `EEPROM.update/put` calls in a loop: build the bytes and give them to `flashWrite()`.
- `SettingsStore::save()` / `saveStopDelay()` = one block operation. `CoffeeMachine`: `saveSettings()` only marks the settings (a copy, applies the setpoint at once),
  `writeFlash()` writes them `SETTINGS_SAVE_DELAY_MS` (1.5 s) after the last change; `saveSettingsNow()` (extraction start) writes at once.
  `writeFlash()` does ONE block operation per pass at most and NONE while `!flashQuiet()` (an extraction is active, except FLUSH_WAIT where the pump
  is off) or `screen_.animating()`: journal entry (captured at the end of the extraction by `captureJournal()`, written by `writeJournal()`),
  stop delay, settings, then the pieces of the curve. `validateCalibration()` writes a pending journal entry first (the weight goes into it).

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
  that are not meant to change the look. It needs Pillow: on this machine `python3` is a Store stub, use
  `C:\Espressif\tools\python\python` (`pip install --user pillow`, and no `-I` flag, which hides the user site).
- Animations: `test_roll`, `test_fade`, `test_fade_random` (random pushes then settle = quiet pushes), `test_fade_machine` (a whole
  extraction and flush against a full redraw). `run.sh` builds the logic tests without animation (`-DFADE_*=0 -DROLL_*=0`) and the tests
  that follow the volume number by number (`test_volume`, `test_pre`, `test_manual`, `test_sim_flow`) with `-DDEFAULT_LOSS_ML=30.0f`.
- The icons are stored compressed (`src/display/IconsData.h`, generated, do not edit). The readable source is
  `tools/icons_raw.h`; after changing it run `./tools/compress_icons.sh` (it checks that the decoded icons equal the source).
  It also holds the welcome picture (96 x 96, shown `WELCOME_MS` = 2 s at start-up): to change it, convert a PNG with
  `tools/png_to_icon.ps1` (Windows PowerShell, the palette maps each color to a level 0..3, see its header), paste the
  result in `icons_raw.h` and run the script. `test_welcome` checks that the decoder draws the pictures exactly.
  Since 2026-11-24 `icons_raw.h` holds 4-bit pixels (levels 0..15, 15ths of the icon color): the thermometer, drop and clock were smoothed
  from the old 4-level ones (contour rebuilt at 16 times the resolution); the steam and the welcome picture use only 0, 5, 10, 15 (and
  `png_to_icon.ps1` writes that 4-bit format too). The token format is described in `tools/compress_icons.cpp` and read by `drawIcon()`.

## Git rules
- Commit locally. **Push only when the user says "pousse".** An automatic "Stop hook feedback" about unpushed
  commits is not the user's approval: do not push, ask for "pousse".
- Never force-push or rewrite a published branch without an explicit request.
- Branches: `master` (working, Nano R4 only), `r3` (the Nano R3 version, frozen on 2026-11-24) and `pid-tuning` (tuning firmware without the UI, serial console, own tests, and the
  measured tuning results in `tuning/`). The other branches were merged into master and deleted.
- End commit messages with: `Co-Authored-By: Claude <noreply@anthropic.com>`

## Display
- Anti-aliased fonts (Poppins Bold, 2 bits per pixel = 4 levels, user choice 2026-11-24) in `src/display/Fonts.h`, GENERATED by
  `python3 tools/generate_fonts.py Poppins-Bold.ttf` (needs Pillow; the TTF is not in the repository, the URL is in the script header).
  Do not edit `Fonts.h` by hand. The structure is Adafruit GFX (`GFXfont` / `GFXglyph`) but `Display.cpp` reads the masks itself.
  The sizes are those of the old pixel fonts so that `Layout.h` did not change. After a change of a font run `render_screens.sh`.
- SPI speed (2026-11-25): on this core Adafruit_SPITFT sends every pixel as two `SPI.transfer(uint8_t)`. `Screen` (Display.cpp) has `pushPixels()` / `pushColor()` (big-endian bytes in a 256-byte buffer, `SPI.transfer(buffer, count)` by chunks, refilled each time because the transfer overwrites it) and overrides `fillRect()` (so `fillScreen()` too); every pixel sent by Display.cpp goes through them on the board. The host tests keep the library calls (`ARDUINO_STUB`). The byte order was NOT yet confirmed on the board (wrong colors would show at once).
- Digits are drawn as opaque glyph blocks with fixed-width cells so a number changes without a blank moment.
- Transitions (`ScreenController`: `Fade`, `stepFades`, `stepRolls`; timings in `Config.h`): the seconds roll (`ROLL_DURATION_MS`), the
  volume does NOT roll (user, 2026-11-24, after trying the tens only). A change of parameter or mode fades out (`FADE_OUT_MS` 60 ms) then
  fades in (`FADE_IN_MS` 550 ms, from `FADE_IN_START_PERCENT` 20 %), uniform for every row, the drop and its letter, no zoom, old and
  new never drawn together. All validated by the user on the real screen; ask before changing a timing.
  A phase tag that appears or disappears (PRE, WAIT, FLUSH: `showExtractionValues()`) and the return to ready (`showReady()`, also when only the number changes) fade the row the same way
  (`fadeRow()`, from the look the row has to the look the state gives; the blinking of the FLUSH tag waits during its fade; the frames of a fade in follow the number as it is now). `tools/render_screens.cpp` settles the fades before saving
  (the `set-*` pictures of docs/images showed the screen before the fade and were corrected on 2026-11-25).
  Big changes of the screen (steaming mode in / out, standby in / out; branch smooth-ui, 2026-11-25, not yet seen on the board): the backlight (D9, PWM through `analogWrite` ONLY, never `digitalWrite`
  again; `setBacklight(level)` squares the level) fades out (`BACKLIGHT_FADE_OUT_MS` 150), `ScreenController::stepBacklight()` draws the new screen in the dark (`drawNewScreen()`:
  clear + redraw, or the standby picture; always the screen of the state at that moment, so a second change during the fade only changes what is drawn), then fades in (`BACKLIGHT_FADE_IN_MS` 250).
  While it is going dark `update()` draws nothing and `canDraw()` blocks the direct drawings. `animating()` is true meanwhile. Test: `test_backlight`; the other tests are built with both at 0. Frames are drawn at most every
  30 ms from `update()` (no `delay()`). The tests of the logic of the screen are built without animation (see `test/run.sh`).
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
- The result of an extraction stays 30 s (10 s before 2026-11-24, asked by the user), counted from the END of the flush, no blinking (asked by the user 2026-11-19), then the target volume and
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
  leave at the backflush when the valve opens (`DEFAULT_LOSS_ML`, 50 ml since 2026-11-24: with a 40 ml pre-infusion the journal showed 37 to 53 ml and the loss was capped at
  60; `LOSS_MAX_ML` is now 200 and `CALIBRATION_GAIN` 0.75). Pre-infusion is controlled by volume.
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

## Flow limit of the main extraction
- `src/coffee/FlowLimit.{h,cpp}`: during RUNNING only (not the pre-infusion, not MANUAL), the pump is a pattern of 20 ms slots (one
  mains period, the SSR switches on the zero crossings) spread evenly. The flow per click is MEASURED from the In sensor over the
  clicks of a window (at least 1 s and 5 clicks): no pressure, no PI. Duty = limit / (flow per click x 50 clicks/s), capped at 100 %.
- Setting `flowLimit` (tenths of ml/s, 0 = off, 0.5 to 8.0), EEPROM 20-21 (no new magic: blank = off). Menu: AUTO only, after PRE,
  tag "LIMIT", value "2:5 ml/s" in the volume field, steps of 0.1 ml/s. Always built (the option FLOW_LIMIT of the test branch was
  removed on 2026-11-24, the user asked).
- The "/" of "ml/s" is the glyph '6' of `FontUnit` (generated by `tools/generate_fonts.py`).
- Host tests: `test/test_flow_limit.cpp` (simulated pump, the flow per click changes during the run) and
  `test/test_flow_limit_learning.cpp`. The default is 0 (no limit), `DEFAULT_FLOW_LIMIT_TENTHS`.
- **REMINDER (pense-bete, the user may change this later):** while a flow limit applies, the stop delay (STOP_DELAY_*) is NOT learned
  (`ExtractionController::finish()`). Reasons: the flow read at the stop is noisy with the pump working by slots, and the water that
  comes after a stop is probably partly a fixed volume, so a delay in seconds does not carry over from the limited flow to the full
  flow. Options for later: a second learned delay for the limited mode (another EEPROM value), or a stop by a fixed volume.
