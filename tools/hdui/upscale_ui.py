"""Enlarges frame and glyph images, on the GPU where one is wanted.

Shape frames go through `realesrgan-ncnn-vulkan`, which runs on any Vulkan GPU;
their masks are enlarged by repeating pixels, since a blended edge would blur the
transparent border the blitter reads. Glyph sheets go through a pixel-art method
rather than a neural one: six-pixel type comes back smeared from ESRGAN.

Nothing here decides what a file is for. `build_hdui_pack.py` drives it.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

import png

ESRGAN = os.environ.get("OPENTS_REALESRGAN", "realesrgan-ncnn-vulkan")
XBRZ = os.environ.get("OPENTS_XBRZ", "")


class UpscaleError(RuntimeError):
    pass


def repeat(image: png.Image, factor: int) -> png.Image:
    """Grows every pixel to a square of the factor."""
    if factor < 1:
        raise UpscaleError("the factor must be at least one")
    if factor == 1:
        return image

    out = png.new(image.width * factor, image.height * factor, image.mode)
    out.palette = image.palette
    channels = image.channels

    for y in range(out.height):
        row = image.row(y // factor)
        line = bytearray()
        for x in range(image.width):
            pixel = row[x * channels:(x + 1) * channels]
            line += pixel * factor
        start = y * out.width * channels
        out.pixels[start:start + len(line)] = line

    return out


def esrgan(source: Path, dest: Path, factor: int = 2, model: str = "realesr-general-x4v3",
           gpu: int = 0, binary: str | None = None) -> Path:
    """Runs the ncnn Real-ESRGAN binary over one image or one folder."""
    program = binary or ESRGAN
    if shutil.which(program) is None:
        raise UpscaleError(
            f"{program} is not on PATH; install it or set OPENTS_REALESRGAN to its full path"
        )

    dest.parent.mkdir(parents=True, exist_ok=True)
    command = [program, "-i", str(source), "-o", str(dest), "-s", str(factor),
               "-n", model, "-f", "png", "-g", str(gpu)]
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        raise UpscaleError(f"{program} failed: {result.stderr.strip() or result.returncode}")
    return dest


def pixel_art(source: Path, dest: Path, factor: int = 2, binary: str | None = None) -> Path:
    """Enlarges a glyph sheet, through an xBRZ-style program when one is named.

    Without one the pixels are repeated, which keeps the type readable and
    exactly on the grid; it is the honest default rather than a silent failure.
    """
    program = binary or XBRZ
    dest.parent.mkdir(parents=True, exist_ok=True)

    if program and shutil.which(program):
        command = [program, "-s", str(factor), str(source), str(dest)]
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode != 0:
            raise UpscaleError(f"{program} failed: {result.stderr.strip() or result.returncode}")
        return dest

    image = png.read(Path(source).read_bytes())
    dest.write_bytes(png.write(repeat(image, factor)))
    return dest


def frames(folder: Path, factor: int = 2, model: str = "realesr-general-x4v3",
           gpu: int = 0) -> int:
    """Enlarges every frame PNG in place, masks by repetition and colour by ESRGAN."""
    folder = Path(folder)
    colors = sorted(p for p in folder.glob("frame*.png") if not p.name.endswith(".mask.png"))
    masks = sorted(folder.glob("frame*.mask.png"))

    for mask in masks:
        image = png.read(mask.read_bytes())
        mask.write_bytes(png.write(repeat(image, factor)))

    if not colors:
        return 0

    with tempfile.TemporaryDirectory() as scratch:
        staging = Path(scratch) / "in"
        staging.mkdir()
        for path in colors:
            shutil.copy2(path, staging / path.name)

        out = Path(scratch) / "out"
        esrgan(staging, out, factor, model, gpu)

        for path in colors:
            produced = out / path.name
            if not produced.exists():
                raise UpscaleError(f"{produced.name} was not produced by the upscaler")
            image = png.read(produced.read_bytes())
            if image.width != png.read(path.read_bytes()).width * factor:
                raise UpscaleError(f"{produced.name} came back at the wrong size")
            path.write_bytes(png.write(image))

    return len(colors)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("folder", type=Path, help="a folder shp2png or fnt2png wrote")
    parser.add_argument("--scale", type=int, default=2)
    parser.add_argument("--model", default="realesr-general-x4v3")
    parser.add_argument("--gpu", type=int, default=0)
    args = parser.parse_args(argv)

    sheet = args.folder / "sheet.png"
    if sheet.exists():
        pixel_art(sheet, sheet, args.scale)
        print(f"enlarged {sheet}")
        return 0

    count = frames(args.folder, args.scale, args.model, args.gpu)
    print(f"enlarged {count} frames in {args.folder}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
