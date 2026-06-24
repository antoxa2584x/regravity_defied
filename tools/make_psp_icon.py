#!/usr/bin/env python3
"""Convert assets/icon.jpg into ICON0.PNG, the 144x80 thumbnail the PSP's Game
menu shows for an EBOOT. pack-pbp embeds it into EBOOT.PBP (see Makefile.psp).

The PSP icon is 144x80 (a 1.8:1 landscape), but the source art is square, so the
square icon is scaled to the 80px height and centered on a black 144x80 canvas.
This is the PSP counterpart to make_3ds_icon.py / make_nds_icon.py.

Run from the repo root:  python3 tools/make_psp_icon.py
"""
import os

from PIL import Image

ASSETS = os.path.join(os.path.dirname(__file__), "..", "assets")
SRC = os.path.join(ASSETS, "icon.jpg")
OUT = os.path.join(ASSETS, "icon_psp.png")

W, H = 144, 80


def main():
    im = Image.open(SRC).convert("RGB")
    # Square-crop the source (icon.jpg is already 1:1, but be safe), scale it to
    # the icon height, and center it on a black 144x80 canvas.
    side = min(im.width, im.height)
    left = (im.width - side) // 2
    top = (im.height - side) // 2
    im = im.crop((left, top, left + side, top + side)).resize((H, H), Image.LANCZOS)

    canvas = Image.new("RGB", (W, H), (0, 0, 0))
    canvas.paste(im, ((W - H) // 2, 0))
    canvas.save(OUT)
    print("wrote", os.path.relpath(OUT))


if __name__ == "__main__":
    main()
