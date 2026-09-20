"""Encodes an enlarged glyph sheet back into a .FNT.

Every glyph's width, height and first row are multiplied by the scale and its
pixels are read out of the sheet cell. Sheet values are snapped back to the
palette indices the source font used, since an upscaler blends between them and a
font carries only the sixteen.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import fnt
import png


class EncodeError(RuntimeError):
    pass


def _snap(value: int, levels: tuple[int, ...]) -> int:
    return min(levels, key=lambda level: abs(level - value))


def build(folder: Path, scale: int, levels: tuple[int, ...] | None = None) -> fnt.Font:
    folder = Path(folder)
    record = json.loads((folder / "font.json").read_text())
    sheet = png.read((folder / "sheet.png").read_bytes())

    if levels is None:
        levels = tuple(record.get("levels") or (0, 1, 2, 3, 4))

    cell_width = record["cell_width"] * scale
    cell_height = record["cell_height"] * scale
    columns = record["columns"]

    if sheet.width < cell_width * columns:
        raise EncodeError(
            f"the sheet is {sheet.width} wide, not the {cell_width * columns} the scale asks for"
        )
    if sheet.mode not in (png.GREY, png.RGB, png.RGBA):
        raise EncodeError(f"a {sheet.mode} sheet cannot be read as glyph values")

    font = fnt.Font(record["max_width"] * scale, record["max_height"] * scale,
                    record.get("compress", 0))

    for index, entry in enumerate(record["glyphs"]):
        glyph = fnt.Glyph(entry["width"] * scale, entry["first_row"] * scale,
                          entry["height"] * scale)

        if glyph.width > 0 and glyph.height > 0:
            origin_x = (index % columns) * cell_width
            origin_y = (index // columns) * cell_height + glyph.first_row
            pixels = bytearray(glyph.width * glyph.height)

            for y in range(glyph.height):
                for x in range(glyph.width):
                    sx, sy = origin_x + x, origin_y + y
                    if sx >= sheet.width or sy >= sheet.height:
                        continue
                    value = sheet.pixel(sx, sy)
                    if sheet.mode != png.GREY:
                        value = value[0]
                    pixels[y * glyph.width + x] = _snap(value, levels)

            glyph.pixels = bytes(pixels)

        font.glyphs.append(glyph)

    return font


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("folder", type=Path, help="the folder fnt2png wrote, with an enlarged sheet")
    parser.add_argument("font", type=Path, help="the .FNT file to write")
    parser.add_argument("--scale", type=int, default=2, help="how far the sheet was enlarged")
    args = parser.parse_args(argv)

    font = build(args.folder, args.scale)
    args.font.parent.mkdir(parents=True, exist_ok=True)
    args.font.write_bytes(fnt.write(font))
    print(f"wrote {args.font} ({len(font.glyphs)} glyphs, {font.max_width}x{font.max_height} cell)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
