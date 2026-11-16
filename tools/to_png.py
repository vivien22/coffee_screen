# Converts the RGB565 frame buffers written by render_screens into round PNG pictures.
# usage: to_png.py <input folder> <output folder>
import os
import struct
import sys

from PIL import Image, ImageDraw

SIZE = 240
SCALE = 2          # 480 px pictures
BORDER = 6         # dark ring around the round screen, as the bezel of the display


def load(path):
    data = open(path, "rb").read()
    pixels = struct.unpack("<%dH" % (SIZE * SIZE), data)
    img = Image.new("RGB", (SIZE, SIZE))
    img.putdata([((p >> 11) * 255 // 31, ((p >> 5) & 0x3F) * 255 // 63, (p & 0x1F) * 255 // 31) for p in pixels])
    return img.resize((SIZE * SCALE, SIZE * SCALE), Image.NEAREST)


def round_picture(img):
    size = SIZE * SCALE
    full = size + 2 * BORDER
    out = Image.new("RGBA", (full, full), (0, 0, 0, 0))
    ImageDraw.Draw(out).ellipse((0, 0, full - 1, full - 1), fill=(34, 34, 38, 255))   # bezel
    mask = Image.new("L", (size * 4, size * 4), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, size * 4 - 1, size * 4 - 1), fill=255)
    mask = mask.resize((size, size), Image.LANCZOS)                                   # smooth edge
    out.paste(img, (BORDER, BORDER), mask)
    return out


src, dst = sys.argv[1], sys.argv[2]
for name in sorted(os.listdir(src)):
    if name.endswith(".rgb565"):
        round_picture(load(os.path.join(src, name))).save(os.path.join(dst, name[:-7] + ".png"), optimize=True)
        print(name[:-7])
