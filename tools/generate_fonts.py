#!/usr/bin/env python3
# Generates src/display/Fonts.h: the anti-aliased fonts of the screen, from Poppins Bold.
#
#   usage: python3 tools/generate_fonts.py <Poppins-Bold.ttf> [output (default src/display/Fonts.h)]
#
# The font is Poppins Bold (SIL Open Font License 1.1, https://github.com/google/fonts/tree/main/ofl/poppins). The TTF file is
# not part of the repository: download it from
#   https://github.com/google/fonts/raw/main/ofl/poppins/Poppins-Bold.ttf
# Needs Python 3 with Pillow (with FreeType).
#
# Each glyph is a mask of 2 bits per pixel (4 levels of opacity, as the icons of tools/icons_raw.h): 0 = background, 3 = full color,
# 4 pixels per byte, the first pixel of a byte in the high bits, the pixels row after row. The glyphs are drawn 8 times larger,
# reduced by the mean of 8 x 8 pixels, then the opacity is rounded to 4 levels. The sizes are chosen to be those of the pixel fonts
# that these ones replace (the height of the ink of a digit: 47 px large, 29 px small; of the "l" of the units: 25 px; of the
# capitals of the tags: 15 px; of the capitals of "ERR": 30 and 45 px), so that the layout of Layout.h does not change.
# The structure is the one of Adafruit GFX (GFXfont / GFXglyph); Display.cpp reads the masks itself (the library draws 1 bit fonts).
#
# The codes of the characters are the ones of the previous fonts (see the comments of the tables): the glyphs start at '0'.
import sys
from PIL import Image, ImageDraw, ImageFont

SS = 8             # the glyphs are drawn SS times larger, then reduced
LEVELS = 4         # 2 bits per pixel
TAG_TRACKING = 0.7  # px taken off the advance of every letter of the tags (the letters of Poppins Bold are a bit wide here)

DIGIT_CELL_SMALL = 26   # the cells of Layout.h: no digit may go beyond its cell
DIGIT_CELL_LARGE = 39


class Face:
    """Poppins Bold at the pixel size that gives an ink height to a reference character."""

    def __init__(self, path, reference, target_height):
        self.path = path
        lo, hi = 8, 1200
        while lo < hi:   # the smallest size (in 1/SS px) whose ink is target_height px high
            mid = (lo + hi) // 2
            box = ImageFont.truetype(path, mid).getbbox(reference, anchor="ls")
            if box[3] - box[1] < target_height * SS:
                lo = mid + 1
            else:
                hi = mid
        self.size = lo
        self.font = ImageFont.truetype(path, lo)

    def advance(self, ch):
        return self.font.getlength(ch) / SS


class Glyph:
    def __init__(self, levels, width, height, advance, x_offset, y_offset):
        self.levels = levels          # width * height values 0..3, row after row
        self.width, self.height = width, height
        self.advance = advance
        self.x_offset, self.y_offset = x_offset, y_offset


def reduce_to_levels(big, width, height):
    """The picture drawn SS times larger -> the levels 0..3 of width x height pixels."""
    small = big.resize((width, height), Image.BOX)
    return [int(round(v / 255 * (LEVELS - 1))) for v in small.getdata()]


def glyph_from_font(face, ch, tracking=0.0):
    """The character ch of the face, in the smallest box of whole pixels around its ink."""
    left, top, right, bottom = face.font.getbbox(ch, anchor="ls")
    x0, y0 = left // SS, top // SS                       # whole pixels around the ink (floor)
    x1, y1 = -(-right // SS), -(-bottom // SS)          # (ceil)
    width, height = x1 - x0, y1 - y0
    big = Image.new("L", (width * SS, height * SS), 0)
    ImageDraw.Draw(big).text((-x0 * SS, -y0 * SS), ch, font=face.font, fill=255, anchor="ls")
    advance = int(round(face.advance(ch) - tracking))
    return Glyph(reduce_to_levels(big, width, height), width, height, advance, x0, y0)


def disc(diameter, box_width, box_height, x_offset, y_offset, advance):
    """A filled disc, in a box (the decimal point, the dot of the heater)."""
    big = Image.new("L", (box_width * SS, box_height * SS), 0)
    cx, cy = box_width * SS / 2, box_height * SS / 2
    r = diameter * SS / 2
    ImageDraw.Draw(big).ellipse((cx - r, cy - r, cx + r, cy + r), fill=255)
    return Glyph(reduce_to_levels(big, box_width, box_height), box_width, box_height, advance, x_offset, y_offset)


def ring(outer, inner, box, x_offset, y_offset, advance):
    """A ring (the degree sign), centered in a square box."""
    big = Image.new("L", (box * SS, box * SS), 0)
    c = box * SS / 2
    d = ImageDraw.Draw(big)
    d.ellipse((c - outer * SS / 2, c - outer * SS / 2, c + outer * SS / 2, c + outer * SS / 2), fill=255)
    d.ellipse((c - inner * SS / 2, c - inner * SS / 2, c + inner * SS / 2, c + inner * SS / 2), fill=0)
    return Glyph(reduce_to_levels(big, box, box), box, box, advance, x_offset, y_offset)


def crop_to_cell(g, cell):
    """A digit is drawn as a block of the width of its cell, centered by its advance (as Display.cpp does, with the C division that
    rounds toward zero): the columns of ink that would go beyond the cell (1 or 2 columns at most: the digits of Poppins Bold are
    wider than the old ones) are cut, so that two digits never overlap and the block of a digit can be replaced alone."""
    left = g.x_offset + int((cell - g.advance) / 2)
    cut_left = max(0, -left)
    cut_right = max(0, left + g.width - cell)
    if cut_left or cut_right:
        rows = [g.levels[r * g.width:(r + 1) * g.width][cut_left:g.width - cut_right] for r in range(g.height)]
        g.levels = [v for row in rows for v in row]
        g.width -= cut_left + cut_right
        g.x_offset += cut_left
        sys.stderr.write("digit cell %d: cut %d + %d columns\n" % (cell, cut_left, cut_right))


def pack(glyphs):
    """The masks of the glyphs: bytes of 4 pixels, every glyph starting on a byte."""
    data, offsets = [], []
    for g in glyphs:
        offsets.append(len(data))
        for i in range(0, len(g.levels), 4):
            chunk = g.levels[i:i + 4] + [0] * (4 - len(g.levels[i:i + 4]))
            data.append((chunk[0] << 6) | (chunk[1] << 4) | (chunk[2] << 2) | chunk[3])
    return data, offsets


def write_font(out, name, first_code, glyphs, names, y_advance, comment):
    data, offsets = pack(glyphs)
    out.write("// %s\n" % comment)
    out.write("const uint8_t %sBitmaps[] PROGMEM = {\n" % name)
    for i in range(0, len(data), 12):
        out.write("  " + ", ".join("0x%02X" % b for b in data[i:i + 12]) + ",\n")
    out.write("};\n\n")
    out.write("const GFXglyph %sGlyphs[] PROGMEM = {\n" % name)
    for i, g in enumerate(glyphs):
        out.write("  { %d, %d, %d, %d, %d, %d }, // 0x%02X %s\n" %
                  (offsets[i], g.width, g.height, g.advance, g.x_offset, g.y_offset, first_code + i, names[i]))
    out.write("};\n\n")
    out.write("const GFXfont %s PROGMEM = {(uint8_t *)%sBitmaps, (GFXglyph *)%sGlyphs, 0x%02X, 0x%02X, %d};\n\n" %
              (name, name, name, first_code, first_code + len(glyphs) - 1, y_advance))


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    path = sys.argv[1]
    target = sys.argv[2] if len(sys.argv) > 2 else "src/display/Fonts.h"

    large = Face(path, "0", 47)
    small = Face(path, "0", 29)
    unit = Face(path, "l", 25)
    slash = Face(path, "/", 21)
    tag = Face(path, "E", 15)
    err_small = Face(path, "E", 30)
    err_large = Face(path, "E", 45)

    digits_large = [glyph_from_font(large, c) for c in "0123456789"]
    digits_small = [glyph_from_font(small, c) for c in "0123456789"]
    # The decimal point (code ':' after the digits): a disc that sits on the baseline, in the cell of the old one (6 px)
    point = disc(5.4, 6, 6, 0, -6, 6)
    for g in digits_large:
        crop_to_cell(g, DIGIT_CELL_LARGE)
    for g in digits_small:
        crop_to_cell(g, DIGIT_CELL_SMALL)

    # Units: C l m s, then the degree sign and the dot of the heater (both centered on the cursor), then the slash of "ml/s"
    units = [glyph_from_font(unit, c) for c in "Clms"]
    units.append(ring(9, 5, 11, -5, -5, 10))        # '4' degree sign: ring of 2 px, 9 px wide, centered on the cursor
    units.append(disc(7, 9, 9, -4, -4, 8))          # '5' heater dot: disc of 7 px, centered on the cursor
    units.append(glyph_from_font(slash, "/"))       # '6' slash of "ml/s"

    # Tags: the 14 letters of the words PRE WAIT LIMIT FLUSH CAL STEAM and of the letters of the mode (A, M)
    tag_letters = "PREWAITFLUSHMC"
    tags = [glyph_from_font(tag, c, TAG_TRACKING) for c in tag_letters]

    errs = [glyph_from_font(err_small, c) for c in "ER"]
    errl = [glyph_from_font(err_large, c) for c in "ER"]

    with open(target, "w", newline="\n") as out:
        out.write("// Auto-generated by tools/generate_fonts.py - do not edit.\n")
        out.write("// Poppins Bold (SIL Open Font License 1.1), anti-aliased: masks of 2 bits per pixel, 4 pixels per byte (the first pixel\n")
        out.write("// in the high bits, row after row), 0 = background ... 3 = full color, as the icons of Layout.h.\n")
        out.write("// The glyph tables are those of Adafruit GFX (offset of the mask in bytes, width, height, advance, xOffset, yOffset).\n")
        out.write("#pragma once\n#include <Adafruit_GFX.h>\n\n")
        write_font(out, "FontSmall", 0x30, digits_small + [point], list("0123456789") + ["':' = decimal point (a disc on the baseline)"],
                   small.size // SS, "Digits 0-9, small size (the ink of a digit is 29 px high), and the decimal point")
        write_font(out, "FontLarge", 0x30, digits_large, list("0123456789"),
                   large.size // SS, "Digits 0-9, large size (47 px high)")
        write_font(out, "FontUnit", 0x30, units,
                   ["'0' = C", "'1' = l", "'2' = m", "'3' = s", "'4' = degree sign, centered on the cursor",
                    "'5' = dot of the heater, centered on the cursor", "'6' = slash of \"ml/s\""],
                   unit.size // SS, "Units: C, l, m, s (the \"l\" is 25 px high), the degree sign, the dot of the heater and the slash")
        out.write("// Phase tag of the duration (\"PRE\", \"WAIT\", \"FLUSH\"), mode letters, \"STEAM\": 15 px capitals, only the 14 letters needed.\n")
        out.write("// The letters use the consecutive codes '0'..'=' instead of their ASCII codes, so the glyph table has 14 entries:\n")
        out.write("// '0'=P '1'=R '2'=E '3'=W '4'=A '5'=I '6'=T '7'=F '8'=L '9'=U ':'=S ';'=H '<'=M '='=C (mode letter in the drop: \"4\" = A,\n")
        out.write("// \"<\" = M; \"=48\" = CAL).   \"PRE\" = \"012\"   \"WAIT\" = \"3456\"   \"FLUSH\" = \"789:;\"   \"LIMIT\" = \"85<56\"   \"STEAM\" = \":624<\"\n")
        out.write("// The advance of each letter is 0.7 px shorter than the one of the font, so that the words are not wider than the old ones.\n")
        write_font(out, "FontTag", 0x30, tags, ["'%s'" % c for c in tag_letters], tag.size // SS,
                   "Tag letters")
        write_font(out, "FontErrorSmall", 0x30, errs, ["'0' = E", "'1' = R"], err_small.size // SS,
                   "\"ERR\" of a probe fault: 30 px capitals (E = '0', R = '1')")
        write_font(out, "FontErrorLarge", 0x30, errl, ["'0' = E", "'1' = R"], err_large.size // SS,
                   "\"ERR\" of a probe fault in the steaming mode: 45 px capitals (E = '0', R = '1')")

    # What the layout needs to know
    def width(glyphs, text, first=0x30):
        return sum(glyphs[ord(c) - first].advance for c in text)
    sys.stderr.write("written %s\n" % target)
    sys.stderr.write("tag widths (advance): PRE %d LIMIT %d WAIT %d FLUSH %d STEAM %d CAL %d\n" % (
        width(tags, "012"), width(tags, "85<56"), width(tags, "3456"), width(tags, "789:;"), width(tags, ":624<"), width(tags, "=48")))
    sys.stderr.write("ERR advance: small %d, large %d\n" % (width(errs, "011"), width(errl, "011")))


main()
