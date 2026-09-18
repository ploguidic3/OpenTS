"""Decodes a .FNT into one glyph sheet PNG and its metadata.

The sheet lays the glyphs out in a grid of the font's widest and tallest glyph,
one column per glyph, with each glyph's ink placed at the row its metrics give.
Pixels carry the palette index the font stores, 0 to 15, straight into the red
channel of a grey image, so a pixel-art upscaler sees flat, distinct values.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import fnt
import png

COLUMNS = 16


def sheet_size(font: fnt.Font) -> tuple[int, int, int]:
    cell_width = max(1, font.max_width)
    cell_height = max(1, font.max_height)
    rows = (len(font.glyphs) + COLUMNS - 1) // COLUMNS
    return cell_width, cell_height, rows


def to_sheet(font: fnt.Font) -> png.Image:
    cell_width, cell_height, rows = sheet_size(font)
    image = png.new(cell_width * COLUMNS, cell_height * rows, png.GREY)

    for index, glyph in enumerate(font.glyphs):
        if not glyph.pixels:
            continue
        origin_x = (index % COLUMNS) * cell_width
        origin_y = (index // COLUMNS) * cell_height + glyph.first_row

        for y in range(glyph.height):
            for x in range(glyph.width):
                value = glyph.pixels[y * glyph.width + x]
                if origin_y + y >= image.height or origin_x + x >= image.width:
                    continue
                image.pixels[(origin_y + y) * image.width + origin_x + x] = value

    return image


def metadata(font: fnt.Font, name: str) -> dict:
    cell_width, cell_height, rows = sheet_size(font)
    return {
        "name": name,
        "max_width": font.max_width,
        "max_height": font.max_height,
        "columns": COLUMNS,
        "rows": rows,
        "cell_width": cell_width,
        "cell_height": cell_height,
        "glyphs": [
            {"width": glyph.width, "first_row": glyph.first_row, "height": glyph.height}
            for glyph in font.glyphs
        ],
    }


def export(source: Path, out_dir: Path) -> Path:
    font = fnt.read(Path(source).read_bytes())
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    (out_dir / "sheet.png").write_bytes(png.write(to_sheet(font)))
    record = out_dir / "font.json"
    record.write_text(json.dumps(metadata(font, Path(source).name), indent=2) + "\n")
    return record


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("font", type=Path, help="the .FNT file to decode")
    parser.add_argument("out_dir", type=Path, help="where the sheet is written")
    args = parser.parse_args(argv)

    record = export(args.font, args.out_dir)
    print(f"wrote {record.parent}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
