#!/bin/sh
# Draws the screens of the machine with the real display code and saves them as PNG pictures
# (docs/images): ./tools/render_screens.sh
# Needs g++ and Python 3 with Pillow. Run it again after a change of the display code.
set -e
cd "$(dirname "$0")/.."
B=${TMPDIR:-/tmp}/coffee_screen_render
rm -rf "$B" && mkdir -p "$B/raw"
cp -r src "$B/src"
# The Arduino stubs of the tests (their Adafruit_GFX draws into a frame buffer)
g++ -std=c++11 -w -I"$B" -Itest/stubs tools/render_screens.cpp \
    "$B/src/display/Display.cpp" "$B/src/display/ScreenController.cpp" -o "$B/render"
"$B/render" "$B/raw"
mkdir -p docs/images
python3 -I tools/to_png.py "$B/raw" docs/images
