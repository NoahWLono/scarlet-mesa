#!/usr/bin/env python3
"""Render the game's printable ASCII atlas from an OFL JetBrains Mono font.

Requires Pillow. Pass --regular/--bold when fonts are installed elsewhere.
The game turns grayscale intensity into texture alpha at load time.
"""

import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
FONT_DIR = Path('/usr/share/fonts/TTF')
CELL_W, CELL_H = 32, 44
COLUMNS, ROWS = 16, 6
FONT_SIZE = 36
ORIGIN_X, BASELINE_Y = 5, 34


def render(font_path: Path, output_path: Path, scale: int = 1) -> dict:
    font = ImageFont.truetype(str(font_path), FONT_SIZE * scale)
    cell_w, cell_h = CELL_W * scale, CELL_H * scale
    atlas = Image.new('L', (COLUMNS * cell_w, ROWS * cell_h), 0)
    draw = ImageDraw.Draw(atlas)
    for codepoint in range(32, 127):
        index = codepoint - 32
        x = (index % COLUMNS) * cell_w + ORIGIN_X * scale
        y = (index // COLUMNS) * cell_h + BASELINE_Y * scale
        draw.text((x, y), chr(codepoint), fill=255, font=font, anchor='ls')
    atlas.convert('RGB').save(output_path)
    return {
        'file': output_path.name,
        'source_font': font_path.name,
        'source_sha256': hashlib.sha256(font_path.read_bytes()).hexdigest(),
        'font_size_px': FONT_SIZE * scale,
        'advance_px': font.getlength('M'),
        'scale': scale,
        'image_size_px': list(atlas.size),
        'cell_size_px': [cell_w, cell_h],
        'glyph_origin_x_px': ORIGIN_X * scale,
        'baseline_y_px': BASELINE_Y * scale,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--regular', type=Path,
                        default=FONT_DIR / 'JetBrainsMonoNerdFontMono-Regular.ttf')
    parser.add_argument('--bold', type=Path,
                        default=FONT_DIR / 'JetBrainsMonoNerdFontMono-Bold.ttf')
    parser.add_argument('--output', type=Path, default=ROOT / 'assets')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    variants = [render(args.regular, args.output / 'font.bmp'),
                render(args.bold, args.output / 'font-bold.bmp'),
                render(args.bold, args.output / 'font-display.bmp', scale=4)]
    metadata = {
        'family': 'JetBrains Mono (ASCII from the Nerd Font Mono build)',
        'copyright': 'Copyright 2020 The JetBrains Mono Project Authors',
        'upstream': 'https://github.com/JetBrains/JetBrainsMono',
        'license': 'SIL Open Font License 1.1; see FONT-LICENSE.txt',
        'image_size_px': [COLUMNS * CELL_W, ROWS * CELL_H],
        'cell_size_px': [CELL_W, CELL_H],
        'columns': COLUMNS,
        'rows': ROWS,
        'first_codepoint': 32,
        'last_codepoint': 126,
        'unused_cell': 95,
        'glyph_origin_x_px': ORIGIN_X,
        'baseline_y_px': BASELINE_Y,
        'pixel_format': 'RGB24 BMP; grayscale white glyphs on black background',
        'texture_conversion': 'Copy grayscale intensity to alpha; set RGB to white.',
        'display_atlas': 'font-display.bmp uses the same layout at 4x resolution.',
        'variants': variants,
    }
    (args.output / 'font-metadata.json').write_text(
        json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(metadata, indent=2))


if __name__ == '__main__':
    main()
