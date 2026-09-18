"""Decodes a SHP into one PNG per frame, a mask per frame, and its metadata.

The PNG carries the frame's colours through the palette given, the mask carries
255 where the frame has a pixel and 0 where it is transparent, and `shape.json`
carries everything the encoder needs to write the file back: the logical size,
each frame's offset, flags and stand-in colour.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import palette as palette_module
import png
import shp


def frame_images(shape: shp.Shape, pal: palette_module.Palette):
    """Yields (index, colour image, mask image) for every frame that has pixels."""
    for index, frame in enumerate(shape.frames):
        if not frame.pixels:
            continue

        color = png.new(frame.width, frame.height, png.RGB)
        mask = png.new(frame.width, frame.height, png.GREY)

        for position, value in enumerate(frame.pixels):
            red, green, blue = pal.colors[value]
            color.pixels[position * 3:position * 3 + 3] = bytes((red, green, blue))
            mask.pixels[position] = 0 if value == 0 else 255

        yield index, color, mask


def metadata(shape: shp.Shape, name: str) -> dict:
    return {
        "name": name,
        "width": shape.width,
        "height": shape.height,
        "flags": shape.flags,
        "frames": [
            {
                "x": frame.x,
                "y": frame.y,
                "width": frame.width,
                "height": frame.height,
                "flags": frame.flags,
                "color": list(frame.color),
                "empty": not frame.pixels,
            }
            for frame in shape.frames
        ],
    }


def export(source: Path, out_dir: Path, pal: palette_module.Palette) -> Path:
    shape = shp.read(Path(source).read_bytes())
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    for index, color, mask in frame_images(shape, pal):
        (out_dir / f"frame{index:04d}.png").write_bytes(png.write(color))
        (out_dir / f"frame{index:04d}.mask.png").write_bytes(png.write(mask))

    record = out_dir / "shape.json"
    record.write_text(json.dumps(metadata(shape, Path(source).name), indent=2) + "\n")
    return record


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("shape", type=Path, help="the .SHP file to decode")
    parser.add_argument("out_dir", type=Path, help="where the frames are written")
    parser.add_argument("--palette", type=Path, required=True, help="the .PAL the shape is drawn through")
    args = parser.parse_args(argv)

    record = export(args.shape, args.out_dir, palette_module.Palette.load(args.palette))
    print(f"wrote {record.parent}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
