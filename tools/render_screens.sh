#!/bin/sh
# Draws the screens of the machine with the real display code and saves them as PNG pictures
# (docs/images): ./tools/render_screens.sh
# Needs g++ and Python 3 with Pillow. Run it again after a change of the display code.
#   HIRES=1 ./tools/render_screens.sh   draws the high resolution look of the Nano R4 (DISPLAY_HIRES) in docs/images/hires
set -e
cd "$(dirname "$0")/.."
B=${TMPDIR:-/tmp}/coffee_screen_render
rm -rf "$B" && mkdir -p "$B/raw"
cp -r src "$B/src"
# The Arduino stubs of the tests (their Adafruit_GFX draws into a frame buffer)
HIRES_FLAG=""
OUT=docs/images
if [ -n "$HIRES" ]; then HIRES_FLAG="-DDISPLAY_HIRES"; OUT=docs/images/hires; fi
g++ -std=c++11 -w $HIRES_FLAG -I"$B" -Itest/stubs tools/render_screens.cpp test/stubs/ArduinoStubs.cpp \
    "$B/src/display/Display.cpp" "$B/src/display/ScreenController.cpp" -o "$B/render"
"$B/render" "$B/raw"
mkdir -p "$OUT"
python3 -I tools/to_png.py "$B/raw" "$OUT"
