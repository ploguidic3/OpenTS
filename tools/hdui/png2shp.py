"""Encodes a folder of frame PNGs back into a SHP at a whole multiple of the size.

Every frame PNG is quantised to the palette given, with the mask deciding which
pixels are transparent, and each frame's offset and size are multiplied by the
scale. A frame the folder has no PNG for is grown from the metadata alone, so a
partially redrawn shape still comes out whole.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import palette as palette_module
import png
import shp


class EncodeError(RuntimeError):
    pass


def _indices(color: png.Image, mask: png.Image | None, pal: palette_module.Palette,
             among: tuple[int, ...]) -> bytes:
    out = bytearray(color.width * color.height)

    for y in range(color.height):
        for x in range(color.width):
            if mask is not None and mask.pixel(x, y) < 128:
                continue
            pixel = color.pixel(x, y)
            if color.mode == png.PALETTE:
                if not color.palette:
                    raise EncodeError("a palette image without a palette cannot be read")
                pixel = color.palette[pixel]
            out[y * color.width + x] = pal.nearest(tuple(pixel[:3]), among)

    return bytes(out)


def build(folder: Path, pal: palette_module.Palette, scale: int,
          among: tuple[int, ...] | None = None) -> shp.Shape:
    folder = Path(folder)
    record = json.loads((folder / "shape.json").read_text())
    candidates = among if among is not None else tuple(range(1, palette_module.COLOR_COUNT))

    shape = shp.Shape(record["width"] * scale, record["height"] * scale, record.get("flags", 0))

    for index, entry in enumerate(record["frames"]):
        frame = shp.Frame(
            x=entry["x"] * scale,
            y=entry["y"] * scale,
            width=entry["width"] * scale,
            height=entry["height"] * scale,
            flags=entry["flags"],
            color=tuple(entry["color"]),
        )

        if entry.get("empty"):
            shape.frames.append(frame)
            continue

        source = folder / f"frame{index:04d}.png"
        if not source.exists():
            raise EncodeError(f"{source.name} is missing and its frame is not marked empty")

        color = png.read(source.read_bytes())
        if color.width != frame.width or color.height != frame.height:
            raise EncodeError(
                f"{source.name} is {color.width}x{color.height}, "
                f"not the {frame.width}x{frame.height} the scale asks for"
            )

        mask_path = folder / f"frame{index:04d}.mask.png"
        mask = png.read(mask_path.read_bytes()) if mask_path.exists() else None
        if mask is not None and (mask.width != color.width or mask.height != color.height):
            raise EncodeError(f"{mask_path.name} is not the size of its frame")

        frame.pixels = _indices(color, mask, pal, candidates)
        shape.frames.append(frame)

    return shape


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("folder", type=Path, help="the folder shp2png wrote, with upscaled frames")
    parser.add_argument("shape", type=Path, help="the .SHP file to write")
    parser.add_argument("--palette", type=Path, required=True, help="the .PAL to quantise to")
    parser.add_argument("--scale", type=int, default=2, help="how far the frames were enlarged")
    args = parser.parse_args(argv)

    shape = build(args.folder, palette_module.Palette.load(args.palette), args.scale)
    args.shape.parent.mkdir(parents=True, exist_ok=True)
    args.shape.write_bytes(shp.write(shape))
    print(f"wrote {args.shape} ({len(shape.frames)} frames at {shape.width}x{shape.height})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
