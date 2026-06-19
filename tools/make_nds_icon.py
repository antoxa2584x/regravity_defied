#!/usr/bin/env python3
"""Convert assets/icon.jpg into the 32x32 BMP that ndstool bakes into the .nds
banner (the icon Nintendo DS/DSi menus show next to the game).

ndstool's banner loader is strict: the bitmap must be exactly 32x32, uncompressed,
and have a 16-colour palette (4bpp on the DS). Pillow writes <=16-colour palettes
as a 4bpp BMP, but this ndstool build wants the 8bpp/16-entry layout used by the
stock calico icon, so we emit that BMP by hand after quantising with Pillow.

Run from the repo root:  python3 tools/make_nds_icon.py
"""
import os
import struct

from PIL import Image

ASSETS = os.path.join(os.path.dirname(__file__), "..", "assets")
SRC = os.path.join(ASSETS, "icon.jpg")
OUT = os.path.join(ASSETS, "nds_icon.bmp")

SIZE = 32
NCOLORS = 16


def main():
    im = Image.open(SRC).convert("RGB")
    # Square-crop (icon.jpg is already 1:1, but be safe) then downscale to 32x32.
    side = min(im.width, im.height)
    left = (im.width - side) // 2
    top = (im.height - side) // 2
    im = im.crop((left, top, left + side, top + side)).resize(
        (SIZE, SIZE), Image.LANCZOS
    )
    # Quantise to exactly 16 colours; ndstool rejects anything else.
    im = im.quantize(colors=NCOLORS, method=Image.MEDIANCUT)

    pal = im.getpalette()[: NCOLORS * 3]
    pal += [0] * (NCOLORS * 3 - len(pal))  # pad to a full 16-entry table
    indices = im.tobytes()  # P-mode: one palette index per pixel, row-major

    palette_bytes = b"".join(
        struct.pack("<BBBB", pal[i * 3 + 2], pal[i * 3 + 1], pal[i * 3], 0)
        for i in range(NCOLORS)
    )  # BMP palette is BGRA

    # 8bpp pixel data, bottom-up rows. 32 px/row is already 4-byte aligned.
    rows = [indices[y * SIZE:(y + 1) * SIZE] for y in range(SIZE)]
    pixels = b"".join(reversed(rows))

    pixel_offset = 14 + 40 + len(palette_bytes)
    file_size = pixel_offset + len(pixels)

    file_header = struct.pack("<2sIHHI", b"BM", file_size, 0, 0, pixel_offset)
    info_header = struct.pack(
        "<IiiHHIIiiII",
        40,            # biSize
        SIZE,          # biWidth
        SIZE,          # biHeight (positive => bottom-up)
        1,             # biPlanes
        8,             # biBitCount (8bpp / 256-colour format, 16 used)
        0,             # biCompression = BI_RGB (uncompressed)
        len(pixels),   # biSizeImage
        0, 0,          # resolution
        NCOLORS,       # biClrUsed = 16
        NCOLORS,       # biClrImportant
    )

    with open(OUT, "wb") as f:
        f.write(file_header + info_header + palette_bytes + pixels)
    print("wrote", os.path.relpath(OUT), "(%d bytes)" % file_size)


if __name__ == "__main__":
    main()
