#!/usr/bin/env python3
"""Convert assets/icon.jpg into the 48x48 PNG that smdhtool bakes into the .smdh
metadata (the icon the 3DS HOME menu / homebrew launcher shows for the app).

smdhtool wants a 48x48 image; it handles the RGB565 conversion itself, so a plain
24-bit PNG is enough. This is the 3DS counterpart to make_nds_icon.py.

Run from the repo root:  python3 tools/make_3ds_icon.py
"""
import os

from PIL import Image

ASSETS = os.path.join(os.path.dirname(__file__), "..", "assets")
SRC = os.path.join(ASSETS, "icon.jpg")
OUT = os.path.join(ASSETS, "icon_3ds.png")

SIZE = 48


def main():
    im = Image.open(SRC).convert("RGB")
    # Square-crop (icon.jpg is already 1:1, but be safe) then downscale to 48x48.
    side = min(im.width, im.height)
    left = (im.width - side) // 2
    top = (im.height - side) // 2
    im = im.crop((left, top, left + side, top + side)).resize(
        (SIZE, SIZE), Image.LANCZOS
    )
    im.save(OUT)
    print("wrote", os.path.relpath(OUT))


if __name__ == "__main__":
    main()
