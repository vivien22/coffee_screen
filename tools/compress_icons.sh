#!/bin/sh
# Rebuilds src/display/IconsData.h (the compressed icons) from tools/icons_raw.h: ./tools/compress_icons.sh
# Needs g++. The generator decodes what it wrote and stops if the icons differ from the source.
set -e
cd "$(dirname "$0")/.."
B=${TMPDIR:-/tmp}/coffee_screen_icons
mkdir -p "$B"
g++ -std=c++11 -w -Itools tools/compress_icons.cpp -o "$B/compress_icons"
"$B/compress_icons" > "$B/IconsData.h"
cp "$B/IconsData.h" src/display/IconsData.h
