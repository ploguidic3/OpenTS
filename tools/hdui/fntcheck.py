"""Checks an enlarged font against the font it was made from.

Every glyph should keep its shape: the metrics multiplied by the scale, each pixel
grown to a square of it, and the same indices drawn with. This reports where that
does not hold, which is what a font that draws as garbage looks like from outside
the game.
"""

from __future__ import annotations

import argparse
from pathlib import Path

import fnt


def compare(source: fnt.Font, grown: fnt.Font, scale: int) -> list[str]:
    """Returns a line per disagreement, empty when the enlargement is faithful."""
    faults: list[str] = []

    if len(grown.glyphs) != len(source.glyphs):
        faults.append(f"glyph count {len(grown.glyphs)}, not the {len(source.glyphs)} it came from")

    if (grown.max_width, grown.max_height) != (source.max_width * scale, source.max_height * scale):
        faults.append(
            f"max {grown.max_width}x{grown.max_height}, not the "
            f"{source.max_width * scale}x{source.max_height * scale} the scale asks for"
        )

    metrics = 0
    pixels = 0

    for index, (before, after) in enumerate(zip(source.glyphs, grown.glyphs)):
        wanted = (before.width * scale, before.first_row * scale, before.height * scale)
        if (after.width, after.first_row, after.height) != wanted:
            if metrics < 5:
                faults.append(
                    f"glyph {index} is {after.width}x{after.height} at row {after.first_row}, "
                    f"not {wanted[0]}x{wanted[2]} at row {wanted[1]}"
                )
            metrics += 1
            continue

        if not before.pixels:
            continue

        for y in range(after.height):
            for x in range(after.width):
                if after.pixels[y * after.width + x] != before.pixels[(y // scale) * before.width + (x // scale)]:
                    if pixels < 5:
                        faults.append(f"glyph {index} differs at {x},{y}")
                    pixels += 1
                    break
            else:
                continue
            break

    if metrics > 5:
        faults.append(f"and {metrics - 5} further glyphs with the wrong metrics")
    if pixels > 5:
        faults.append(f"and {pixels - 5} further glyphs with the wrong pixels")

    source_levels = sorted({value for glyph in source.glyphs for value in glyph.pixels})
    grown_levels = sorted({value for glyph in grown.glyphs for value in glyph.pixels})
    if grown_levels != source_levels:
        faults.append(f"draws with {grown_levels}, not the {source_levels} it came from")

    return faults


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="the font the pack was built from")
    parser.add_argument("grown", type=Path, help="the font in the pack")
    parser.add_argument("--scale", type=int, default=2)
    args = parser.parse_args()

    source = fnt.read(args.source.read_bytes())
    grown = fnt.read(args.grown.read_bytes())

    faults = compare(source, grown, args.scale)

    if not faults:
        print(f"{args.grown}: {len(grown.glyphs)} glyphs, faithful at {args.scale}x")
        return 0

    print(f"{args.grown}:")
    for fault in faults:
        print(f"  {fault}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
