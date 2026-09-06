#!/usr/bin/env python3
"""Convert the generated title illustration into SDL's portable RGB BMP.

This only changes file format. It does not repaint, crop, or scale artwork.
Requires Pillow; run from any directory.
"""

from pathlib import Path

from PIL import Image


assets = Path(__file__).resolve().parents[1] / 'assets'
with Image.open(assets / 'title.png') as image:
    image.convert('RGB').save(assets / 'title.bmp')
