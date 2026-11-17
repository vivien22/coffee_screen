# coffee_screen: working rules

Espresso-machine controller (Ascaso Steel Uno Pro) on an Arduino Nano (ATmega328P): GC9A01 round display,
CJMCU-111 encoder, MAX31865 + PT100, PID heater, pump and 3-way valve relays, two flow sensors.
The README describes the behaviour. The user writes in French: reply in French.

## Build (PlatformIO, env `nanoatmega328new`)
- `pio run` compiles; the line "Flash: ... used N bytes from 30720 bytes" is what matters (limit 30720 B, RAM 2 KB).
- **`build_flags` in `platformio.ini` must keep `-mcall-prologues`** (about 1 KB saved) **and `-fno-ipa-cp -mstrict-X
  -fno-gcse -fshort-enums`** (1262 B saved together, measured one by one then combined). `-flto`, `-fno-jump-tables`,
  `-ffunction-sections`, `-Wl,--gc-sections` brought nothing (PlatformIO already does LTO and gc-sections): do not add
  them. `-finline-limit=3 -fno-reorder-blocks` would give 154 B more: left out, they slow the inlined loops.
- Flash is tight: report the size after every change that touches the display, the machine or the settings.
- Last measured (flags above, compressed icons): 28568 B flash, 456 B RAM with `FLOW_SIMULATOR` on (what config.h has);
  28756 B flash, 463 B RAM with the real flow sensors (`FLOW_SIMULATOR` commented out).

## Tests (host, no Arduino)
- `./test/run.sh` runs all tests (needs bash and g++; on Windows use WSL or Git Bash).
- `test_display` is a property test (incremental drawing must equal a fresh draw).
- `./tools/render_screens.sh` regenerates the README PNGs; they must stay identical after display changes
  that are not meant to change the look.
- The icons are stored compressed (`src/display/icons_data.h`, generated, do not edit). The readable source is
  `tools/icons_raw.h`; after changing it run `./tools/compress_icons.sh` (it checks that the decoded icons equal the source).

## Git rules
- Commit locally. **Push only when the user says "pousse".** An automatic "Stop hook feedback" about unpushed
  commits is not the user's approval: do not push, ask for "pousse".
- Never force-push or rewrite a published branch without an explicit request.
- Branches: `master` (working), `pid-tuning` (tuning firmware without the UI, serial console, own tests),
  `preinfusion-time` (old), `phase-table` (merged into master).
- End commit messages with: `Co-Authored-By: Claude <noreply@anthropic.com>`

## Display
- Custom bit-packed Adafruit GFX fonts (`src/display/fonts.h`), already reduced to the glyphs in use.
- Digits are drawn as opaque glyph blocks with fixed-width cells so a number changes without a blank moment.
- Limits that keep the layout valid: volume <= 99, pre-infusion <= 50, wait and flush <= 9 s, manual <= 99 s.
