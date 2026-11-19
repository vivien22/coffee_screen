#!/usr/bin/env python3
"""make_hires_assets.py: generates src/display/HiResData.h, the anti-aliased (8-bit) fonts and icons and the full colour
welcome picture used when DISPLAY_HIRES is defined (the Nano R4: plenty of flash; the Nano R3 keeps the compact 1-bit fonts
and 2-bit icons of Fonts.h / IconsData.h).

    python tools/make_hires_assets.py --font Poppins-Bold.ttf [--welcome picture.png]

Needs Python 3 with Pillow and numpy. The font is Poppins Bold (SIL Open Font License, see the README); only the rendered
bitmaps are kept in the repository, not the font file.

What it does
  Fonts   every glyph of Fonts.h (the digits in two sizes, the units, the phase tags) is rendered again from the font file,
          at 8 times the size and reduced with a box filter (8-bit coverage = anti-aliased edges). The size and the weight are
          fitted so that the glyphs have the size of the 1-bit ones (same layout: Layout.h does not change), and each glyph keeps
          its advance and its place on the baseline.
  Icons   the thermometer, the drop and the clock are in tools/icons_raw.h as 4 levels only (the first PNG is not kept in the
          repository). Their outline is rebuilt at 8 times the size (smooth upscaling of the 4 levels, then the contour is
          sharpened around each level) and reduced again: 256 levels of coverage instead of 4, smoother curves, same shapes.
  Welcome the same reconstruction at 160 x 160 pixels, in full colour (RGB565) on the background colour. Give --welcome with a
          real picture (PNG, any size, transparent or dark background) to make it from the picture instead: that is the
          way to get real details.
"""
import argparse
import math
import os
import re
import sys
import xml.etree.ElementTree as ET

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFont

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SS = 8                                   # supersampling of the glyphs and of the icons
WELCOME_SIZE = 160


# ---------------------------------------------------------------------------------------------------------------- inputs
def read_old_fonts():
    """The 1-bit fonts of Fonts.h: {name: (bitmap bytes, [(offset, w, h, advance, xOffset, yOffset)])}."""
    text = open(os.path.join(ROOT, "src/display/Fonts.h"), encoding="utf-8").read()
    fonts = {}
    for name in ("FontSmall", "FontLarge", "FontUnit", "FontTag"):
        bits = re.search(r"const uint8_t %sBitmaps\[\] PROGMEM = \{(.*?)\};" % name, text, re.S).group(1)
        data = bytes(int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", bits))
        table = re.search(r"const GFXglyph %sGlyphs\[\] PROGMEM = \{(.*?)\};" % name, text, re.S).group(1)
        glyphs = [tuple(int(v) for v in m.groups()) for m in
                  re.finditer(r"\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(-?\d+),\s*(-?\d+)\s*\}", table)]
        fonts[name] = (data, glyphs)
    return fonts


def read_colors():
    text = open(os.path.join(ROOT, "src/display/Layout.h"), encoding="utf-8").read()
    def rgb(name):
        v = int(re.search(r"#define %s\s+0x([0-9A-Fa-f]+)" % name, text).group(1), 16)
        return ((v >> 11) & 31, (v >> 5) & 63, v & 31), v
    return rgb("COLOR_BG"), rgb("COLOR_ICON")


def read_icons():
    """The 2-bit icons of tools/icons_raw.h: {name: 2-D array of levels 0..3}."""
    text = open(os.path.join(ROOT, "tools/icons_raw.h"), encoding="utf-8").read()
    icons = {}
    for name in ("THERMOMETER", "DROP", "CLOCK", "WELCOME"):
        w = int(re.search(r"#define ICON_%s_WIDTH (\d+)" % name, text).group(1))
        h = int(re.search(r"#define ICON_%s_HEIGHT (\d+)" % name, text).group(1))
        body = re.search(r"const uint8_t ICON_%s\[\] PROGMEM = \{(.*?)\};" % name, text, re.S).group(1)
        data = [int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", body)]
        levels = np.zeros(w * h, dtype=np.uint8)
        for i in range(w * h):
            levels[i] = (data[i // 4] >> (6 - 2 * (i % 4))) & 3
        icons[name] = levels.reshape(h, w)
    return icons


# ---------------------------------------------------------------------------------------------------------------- glyphs
class Face:
    """A font at a given size, rendered at SS times the size."""
    def __init__(self, path, size_px, stroke_px):
        self.font = ImageFont.truetype(path, int(round(size_px * SS)))
        self.stroke = int(round(stroke_px * SS))

    def size_of(self, ch):
        """Width and height in pixels of the ink of a character (fast: no rendering)."""
        l, t, r, b = self.font.getbbox(ch, anchor="ls", stroke_width=self.stroke)
        return (r - l) / SS, (b - t) / SS

    def area(self, ch):
        """Area of the ink of a character, in square pixels (how heavy the glyph is)."""
        big, base, box = self.ink(ch)
        return float(np.asarray(big, dtype=np.float32).sum()) / 255.0 / (SS * SS)

    def ink(self, ch):
        """Mask (SS times larger) of a character with the baseline at row 'base', and its ink box."""
        big = Image.new("L", (self.font.size * 3, self.font.size * 3), 0)
        base = self.font.size * 2
        ImageDraw.Draw(big).text((self.font.size // 2, base), ch, font=self.font, fill=255, anchor="ls",
                                 stroke_width=self.stroke, stroke_fill=255)
        box = big.getbbox()
        return big, base, box


def old_area(data, glyph):
    off, w, h = glyph[0], glyph[1], glyph[2]
    return float(sum(1 for i in range(w * h) if data[(off * 8 + i) // 8] & (0x80 >> ((off * 8 + i) % 8))))


def fit_face(path, data, glyphs, chars):
    """The size and the weight that give the glyphs of the old font their width, their height and their weight (the
    area of the ink: the old glyphs are Poppins Bold emboldened, heavier than the font itself)."""
    def error(size, stroke, with_area=False):
        face = Face(path, size, stroke)
        err = 0.0
        for ch, glyph in zip(chars, glyphs):
            iw, ih = face.size_of(ch)
            err += (iw - glyph[1]) ** 2 + (ih - glyph[2]) ** 2
            if with_area:
                err += 0.02 * (face.area(ch) - old_area(data, glyph)) ** 2
        return err

    best = min((error(s, k), s, k) for s in np.arange(10, 121, 1.0) for k in (0.0, 0.5, 1.0, 1.5))
    _, size0, stroke0 = best
    return min((error(s, k, True), s, k) for s in np.arange(size0 - 3, size0 + 1.01, 0.25)
               for k in np.arange(0, 2.51, 0.25))


def render_glyph(face, ch, old, scale=1.0):
    """The A8 bitmap of one glyph, with the advance and the place of the old one. Returns (w, h, adv, xo, yo, bytes)."""
    off, w, h, adv, xo, yo = old
    big, base, box = face.ink(ch)
    ink_w = (box[2] - box[0]) / SS
    ink_h = (box[3] - box[1]) / SS
    top = (base - box[1]) / SS                 # rows of ink above the baseline
    bottom = (box[3] - base) / SS              # and below it
    # horizontal: the ink is centered where the old one was
    center = xo + w / 2.0
    left = center - ink_w / 2.0
    xo_new = int(math.floor(left)) - 1
    frac = left - math.floor(left) + 1.0       # position of the ink in the bitmap, in pixels
    rows_above = int(math.ceil(top)) + 1
    rows_below = int(math.ceil(max(bottom, 0))) + 1
    wb = int(math.ceil(frac + ink_w)) + 1
    hb = rows_above + rows_below
    canvas = Image.new("L", (wb * SS, hb * SS), 0)
    dx = int(round(frac * SS)) - box[0]
    dy = rows_above * SS - base
    canvas.paste(big.crop(box), (box[0] + dx, box[1] + dy))
    return trimmed(canvas.resize((wb, hb), Image.BOX), adv, xo_new, -rows_above)


def trimmed(img, adv, xo, yo):
    """The bitmap without its empty rows and columns (the offsets follow): no useless byte, and a glyph never goes
    beyond its cell because of a margin."""
    box = img.getbbox()
    if box is None:
        return 1, 1, adv, xo, yo, b"\x00"
    img = img.crop(box)
    return img.width, img.height, adv, xo + box[0], yo + box[1], bytes(img.tobytes())


def disc(diameter, ring=0.0):
    """A disc (or a ring of the given thickness) of the given diameter, anti-aliased; the bitmap has a 1 px margin."""
    n = int(math.ceil(diameter)) + 2
    img = Image.new("L", (n * SS, n * SS), 0)
    d = ImageDraw.Draw(img)
    c = n * SS / 2.0
    r = diameter * SS / 2.0
    d.ellipse((c - r, c - r, c + r, c + r), fill=255)
    if ring:
        r2 = r - ring * SS
        d.ellipse((c - r2, c - r2, c + r2, c + r2), fill=0)
    return img.resize((n, n), Image.BOX)


def make_fonts(path, old):
    """{name: [glyph]} for the four fonts; a glyph is (w, h, advance, xOffset, yOffset, alpha bytes)."""
    out = {}
    digits = "0123456789"
    for name in ("FontSmall", "FontLarge"):
        data, glyphs = old[name]
        size, stroke = fit_face(path, data, glyphs[:10], digits)[1:]
        print("%s: Poppins Bold at %.1f px, stroke %.2f px" % (name, size, stroke), file=sys.stderr)
        face = Face(path, size, stroke)
        if name == "FontSmall":
            weight = stroke / size                       # the weight of the units follows the one of the small digits
        rendered = [render_glyph(face, ch, glyphs[i]) for i, ch in enumerate(digits)]
        if name == "FontSmall":
            # ':' is the decimal point: a disc of radius 2.5, on the baseline (old glyph: 5 x 5, advance 6)
            off, w, h, adv, xo, yo = glyphs[10]
            rendered.append(trimmed(disc(5.0), adv, xo - 1, yo - 1))
        out[name] = rendered

    # Tags: capitals of the codes '0'..'=' (see Fonts.h): P R E W A I T F L U S H M C
    letters = "PREWAITFLUSHMC"
    data, glyphs = old["FontTag"]
    size, stroke = fit_face(path, data, glyphs, letters)[1:]
    print("FontTag: Poppins Bold at %.1f px, stroke %.2f px" % (size, stroke), file=sys.stderr)
    face = Face(path, size, stroke)
    out["FontTag"] = [render_glyph(face, ch, glyphs[i]) for i, ch in enumerate(letters)]

    # "ERR" of a sensor fault: the capitals of the tag at twice the size (codes '0'..'2' = P R E, as in FontTag)
    face2 = Face(path, size * 2, stroke * 2)
    twice = [(0, w * 2, h * 2, adv * 2, xo * 2, yo * 2) for (off, w, h, adv, xo, yo) in glyphs[:3]]
    out["FontError"] = [render_glyph(face2, ch, twice[i]) for i, ch in enumerate(letters[:3])]

    # Units: '0' C, '1' l, '2' m, '3' s each fitted to its own height, '4' degree sign (ring), '5' heater dot (disc)
    data, glyphs = old["FontUnit"]
    units = []
    for ch, old_glyph in zip("Clms", glyphs[:4]):
        best = None
        for size in np.arange(14, 70, 0.25):
            stroke = weight * size
            face = Face(path, size, stroke)
            iw, ih = face.size_of(ch)
            err = abs(ih - old_glyph[2]) + 0.3 * abs(iw - old_glyph[1])
            if best is None or err < best[0]:
                best = (err, size, stroke)
        units.append(render_glyph(Face(path, best[1], best[2]), ch, old_glyph))
    off, w, h, adv, xo, yo = glyphs[4]
    units.append(trimmed(disc(9.0, ring=2.2), adv, xo - 1, yo - 1))   # degree sign: ring of 9 px, centered on the cursor
    off, w, h, adv, xo, yo = glyphs[5]
    units.append(trimmed(disc(7.0), adv, xo - 1, yo - 1))             # heater dot: disc of 7 px
    out["FontUnit"] = units
    return out


# ---------------------------------------------------------------------------------------------------------------- svg
NUMBER = r"[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?"


def flatten_path(d, steps=24):
    """The subpaths of an SVG path (M m L l H h V v C c S s Z z) as lists of points."""
    tokens = re.findall(r"[MmLlHhVvCcSsZz]|" + NUMBER, d)
    subpaths, current = [], []
    x = y = x0 = y0 = 0.0
    last_control = None
    i = 0
    cmd = None

    def number():
        nonlocal i
        v = float(tokens[i])
        i += 1
        return v

    while i < len(tokens):
        if re.match(r"[A-Za-z]", tokens[i]):
            cmd = tokens[i]
            i += 1
            if cmd in "Zz":
                if current:
                    subpaths.append(current)
                current = []
                x, y = x0, y0
                last_control = None
                continue
        rel = cmd.islower()
        c = cmd.upper()
        if c == "M":
            nx, ny = number(), number()
            if rel:
                nx, ny = x + nx, y + ny
            if current:
                subpaths.append(current)
            current = [(nx, ny)]
            x, y, x0, y0 = nx, ny, nx, ny
            cmd = "l" if rel else "L"          # the next pairs of an M are lines
            last_control = None
        elif c == "L":
            nx, ny = number(), number()
            if rel:
                nx, ny = x + nx, y + ny
            current.append((nx, ny))
            x, y = nx, ny
            last_control = None
        elif c == "H":
            nx = number() + (x if rel else 0)
            current.append((nx, y))
            x = nx
            last_control = None
        elif c == "V":
            ny = number() + (y if rel else 0)
            current.append((x, ny))
            y = ny
            last_control = None
        elif c in "CS":
            if c == "C":
                x1, y1 = number(), number()
                if rel:
                    x1, y1 = x + x1, y + y1
            else:
                x1, y1 = (2 * x - last_control[0], 2 * y - last_control[1]) if last_control else (x, y)
            x2, y2, nx, ny = number(), number(), number(), number()
            if rel:
                x2, y2, nx, ny = x + x2, y + y2, x + nx, y + ny
            for k in range(1, steps + 1):
                t = k / steps
                u = 1 - t
                current.append((u ** 3 * x + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t ** 3 * nx,
                                u ** 3 * y + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t ** 3 * ny))
            last_control = (x2, y2)
            x, y = nx, ny
        else:
            raise ValueError("unsupported path command " + cmd)
    if current:
        subpaths.append(current)
    return subpaths


def render_svg(path, size, ink_of, bg, icon):
    """A flat SVG (ellipses, circles, rects and paths with plain fills, like the Twemoji ones) on the background colour,
    size x size pixels, anti-aliased by SS x SS samples per pixel. Every fill colour is turned into an 'ink' 0..1 by
    ink_of(r, g, b), then into a colour between the background and the icon color: the picture takes the tones of the screen."""
    root = ET.parse(path).getroot()
    view = [float(v) for v in root.get("viewBox").split()]
    big = size * SS
    scale = big / max(view[2], view[3])
    canvas = Image.new("RGB", (big, big), bg)

    def px(point):
        return ((point[0] - view[0]) * scale, (point[1] - view[1]) * scale)

    for el in root.iter():
        tag = el.tag.split("}")[-1]
        fill = el.get("fill")
        if tag not in ("ellipse", "circle", "rect", "path") or not fill:
            continue
        mask = Image.new("1", (big, big), 0)
        if tag in ("ellipse", "circle"):
            cx, cy = float(el.get("cx")), float(el.get("cy"))
            rx = float(el.get("rx", el.get("r")))
            ry = float(el.get("ry", el.get("r")))
            (x1, y1), (x2, y2) = px((cx - rx, cy - ry)), px((cx + rx, cy + ry))
            ImageDraw.Draw(mask).ellipse((x1, y1, x2, y2), fill=1)
        elif tag == "rect":
            x, y, w, h = (float(el.get(k)) for k in ("x", "y", "width", "height"))
            (x1, y1), (x2, y2) = px((x, y)), px((x + w, y + h))
            ImageDraw.Draw(mask).rectangle((x1, y1, x2, y2), fill=1)
        else:
            for sub in flatten_path(el.get("d")):          # even-odd: each subpath flips the inside (a hole stays a hole)
                part = Image.new("1", (big, big), 0)
                ImageDraw.Draw(part).polygon([px(p) for p in sub], fill=1)
                mask = ImageChops.logical_xor(mask, part)
        r, g, b = (int(fill[1 + 2 * k:3 + 2 * k], 16) for k in range(3))
        ink = ink_of(r, g, b)
        color = tuple(int(round(bg[k] + (icon[k] - bg[k]) * ink)) for k in range(3))
        canvas.paste(color, mask=mask.convert("L"))
    return canvas.resize((size, size), Image.BOX)


# ---------------------------------------------------------------------------------------------------------------- icons
def gaussian_blur(arr, sigma):
    """Gaussian blur of a 2-D float array (separable, sigma in pixels, edges repeated)."""
    radius = int(math.ceil(3 * sigma))
    kernel = np.exp(-0.5 * (np.arange(-radius, radius + 1) / sigma) ** 2)
    kernel /= kernel.sum()
    for axis in (0, 1):
        pad = [(0, 0), (0, 0)]
        pad[axis] = (radius, radius)
        padded = np.pad(arr, pad, mode="edge")
        out = np.zeros_like(arr)
        for i, k in enumerate(kernel):
            out += k * (padded[i:i + arr.shape[0], :] if axis == 0 else padded[:, i:i + arr.shape[1]])
        arr = out
    return arr


def smooth_levels(levels, factor, sigma):
    """Continuous 'ink' (0..1) at 'factor' times the size from a 4-level picture: the levels are upscaled smoothly and
    blurred a little (sigma, in pixels of the original: it irons out the steps of the contours), then each of the three
    contours (between levels 0|1, 1|2, 2|3) is sharpened around its middle."""
    h, w = levels.shape
    img = Image.fromarray(levels.astype(np.float32) / 3.0)
    up = gaussian_blur(np.asarray(img.resize((w * factor, h * factor), Image.BICUBIC), dtype=np.float32), sigma * factor)
    ink = np.zeros_like(up)
    width = 0.07 * 1.0                                   # transition width of a contour, in ink units (steep: sharp edge)
    for k in range(3):
        t = (k + 0.5) / 3.0
        x = np.clip((up - t) / width * 0.5 + 0.5, 0, 1)
        ink += (x * x * (3 - 2 * x)) / 3.0
    return np.clip(ink, 0, 1)


def reduce_box(arr, factor):
    h, w = arr.shape
    return arr.reshape(h // factor, factor, w // factor, factor).mean(axis=(1, 3))


def make_icon(levels):
    """A8 coverage of an icon from its 4 levels."""
    ink = smooth_levels(levels, SS, 0.45)
    return np.clip(np.rint(reduce_box(ink, SS) * 255), 0, 255).astype(np.uint8)


def make_welcome(levels, picture, bg_rgb888, icon_rgb888):
    """RGB565 picture, WELCOME_SIZE x WELCOME_SIZE, on the background colour."""
    n = WELCOME_SIZE
    if picture and picture.lower().endswith(".svg"):
        # The picture takes the tones of the screen: each color of the SVG gets an "ink" (0 = background, 1 = the icon
        # color); the colors of the Twemoji cup are listed (the same tones as the first 96 x 96 picture), any other color
        # gets its brightness.
        inks = {(0xF5, 0xF8, 0xFA): 1.0,    # the cup
                (0xCC, 0xD6, 0xDD): 0.70,   # the rim, the handle and the saucer
                (0x99, 0xAA, 0xB5): 0.45,   # the edge of the saucer
                (0x8A, 0x4B, 0x38): 0.30,   # the coffee
                (0xD9, 0x9E, 0x82): 0.85}   # the steam
        tone = lambda r, g, b: inks.get((r, g, b), (0.299 * r + 0.587 * g + 0.114 * b) / 255.0)
        rgb = np.asarray(render_svg(picture, n, tone, bg_rgb888, icon_rgb888), dtype=np.float32)
    elif picture:
        img = Image.open(picture).convert("RGBA")
        side = max(img.size)
        square = Image.new("RGBA", (side, side), bg_rgb888 + (255,))
        square.paste(img, ((side - img.width) // 2, (side - img.height) // 2), img)
        rgb = np.asarray(square.convert("RGB").resize((n, n), Image.LANCZOS), dtype=np.float32)
    else:
        factor = 10                                       # 96 x 10 = 960 = WELCOME_SIZE x 6
        ink = smooth_levels(levels, factor, 0.5)
        small = np.asarray(Image.fromarray((ink * 255).astype(np.uint8), "L").resize((n, n), Image.BOX),
                           dtype=np.float32)[..., None] / 255.0
        bg = np.array(bg_rgb888, dtype=np.float32)
        fg = np.array(icon_rgb888, dtype=np.float32)
        rgb = bg + (fg - bg) * small
    r = (np.clip(rgb[..., 0], 0, 255) / 255.0 * 31 + 0.5).astype(np.uint16)
    g = (np.clip(rgb[..., 1], 0, 255) / 255.0 * 63 + 0.5).astype(np.uint16)
    b = (np.clip(rgb[..., 2], 0, 255) / 255.0 * 31 + 0.5).astype(np.uint16)
    return (r << 11) | (g << 5) | b


# ---------------------------------------------------------------------------------------------------------------- output
def c_bytes(data, per_line=24):
    lines = []
    for i in range(0, len(data), per_line):
        lines.append("  " + ", ".join("0x%02X" % b for b in data[i:i + per_line]) + ",")
    return "\n".join(lines)


def write_header(path, fonts, icons, welcome, welcome_note):
    out = []
    out.append("// Auto-generated by tools/make_hires_assets.py: do not edit.")
    out.append("// Anti-aliased fonts (Poppins Bold, SIL Open Font License) and icons, 8-bit coverage, and the full colour welcome")
    out.append("// picture. Used when DISPLAY_HIRES is defined (Display.cpp). %s" % welcome_note)
    out.append("#pragma once")
    out.append("#include <stdint.h>")
    out.append("")
    out.append("// A glyph: its bitmap (width x height bytes of coverage 0..255, row by row) starts at 'offset' in the font data;")
    out.append("// xOffset / yOffset: the top left corner of the bitmap from the cursor, on the baseline; advance: where the cursor goes.")
    out.append("struct HiGlyph { uint16_t offset; uint8_t width, height, advance; int8_t xOffset, yOffset; };")
    out.append("struct HiFont { const uint8_t *alpha; const HiGlyph *glyphs; uint8_t first, last; };")
    out.append("")
    first = {"FontSmall": "0", "FontLarge": "0", "FontUnit": "0", "FontTag": "0", "FontError": "0"}
    last = {"FontSmall": ":", "FontLarge": "9", "FontUnit": "5", "FontTag": "=", "FontError": "2"}
    for name, glyphs in fonts.items():
        blob = b"".join(g[5] for g in glyphs)
        out.append("const uint8_t Hi%sAlpha[] PROGMEM = {" % name)
        out.append(c_bytes(blob))
        out.append("};")
        out.append("const HiGlyph Hi%sGlyphs[] PROGMEM = {" % name)
        offset = 0
        for i, g in enumerate(glyphs):
            w, h, adv, xo, yo, data = g
            out.append("  { %d, %d, %d, %d, %d, %d }, // %s" % (offset, w, h, adv, xo, yo, chr(ord(first[name]) + i)))
            offset += len(data)
        out.append("};")
        out.append("const HiFont Hi%s PROGMEM = { Hi%sAlpha, Hi%sGlyphs, '%s', '%s' };" % (name, name, name, first[name], last[name]))
        out.append("")
    out.append("// Icons: coverage of the icon color over the background, 0..255")
    for name, arr in icons.items():
        h, w = arr.shape
        out.append("#define HI_ICON_%s_WIDTH %d" % (name, w))
        out.append("#define HI_ICON_%s_HEIGHT %d" % (name, h))
        out.append("const uint8_t HI_ICON_%s[] PROGMEM = {" % name)
        out.append(c_bytes(arr.tobytes()))
        out.append("};")
    out.append("")
    out.append("// Welcome picture: RGB565 on the background color, row by row")
    out.append("#define HI_WELCOME_WIDTH %d" % welcome.shape[1])
    out.append("#define HI_WELCOME_HEIGHT %d" % welcome.shape[0])
    out.append("const uint16_t HI_WELCOME[] PROGMEM = {")
    flat = welcome.flatten()
    for i in range(0, len(flat), 12):
        out.append("  " + ", ".join("0x%04X" % v for v in flat[i:i + 12]) + ",")
    out.append("};")
    open(path, "w", encoding="utf-8", newline="\n").write("\n".join(out) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--font", required=True, help="Poppins-Bold.ttf")
    ap.add_argument("--welcome", default=os.path.join(ROOT, "tools/welcome_source.svg"),
                    help="SVG or PNG of the welcome picture (default: tools/welcome_source.svg; 'none': rebuilt from the "
                         "96 x 96 picture of tools/icons_raw.h)")
    ap.add_argument("--out", default=os.path.join(ROOT, "src/display/HiResData.h"))
    args = ap.parse_args()

    (bg_565, _), (icon_565, _) = read_colors()
    to888 = lambda c: (round(c[0] * 255 / 31), round(c[1] * 255 / 63), round(c[2] * 255 / 31))
    old = read_old_fonts()
    fonts = make_fonts(args.font, old)
    raw = read_icons()
    icons = {n: make_icon(raw[n]) for n in ("THERMOMETER", "DROP", "CLOCK")}
    source = None if args.welcome == "none" or not os.path.exists(args.welcome) else args.welcome
    welcome = make_welcome(raw["WELCOME"], source, to888(bg_565), to888(icon_565))
    note = ("The welcome picture comes from " + os.path.basename(source) if source
            else "The welcome picture is the 96 x 96 one of tools/icons_raw.h, rebuilt smoothly at %d x %d." % (WELCOME_SIZE, WELCOME_SIZE))
    write_header(args.out, fonts, icons, welcome, note)
    total = sum(len(g[5]) for f in fonts.values() for g in f) + sum(a.size for a in icons.values()) + welcome.size * 2
    print("wrote %s (%d bytes of data)" % (args.out, total), file=sys.stderr)


if __name__ == "__main__":
    main()
