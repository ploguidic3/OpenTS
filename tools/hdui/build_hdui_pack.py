"""Builds a folder of enlarged interface artwork and the HDPACK.INI that declares it.

The run is: pull the named members out of the archives, decode each into frames or
a glyph sheet, enlarge them, encode them back at the new size, and write the
manifest. Each stage can be run on its own; this is the order they go in.

Nothing here reads the retail data except through the archives named on the
command line, and nothing is written outside the destination folder.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import tempfile

import fnt
import fnt2png
import mixextract
import palette as palette_module
import png2fnt
import png2shp
import png
import shp
import shp2png
import upscale_ui

# Every fixed name the interface draws through, from the fetches in code/sidebar.cpp,
# code/radar.cpp, code/tab.cpp, code/power.cpp and code/mouse.cpp.
SHAPES = (
    "SIDE1.SHP", "SIDE2.SHP", "SIDE3.SHP", "ADDON.SHP",
    "SIDEGDI1.SHP", "SIDEGDI2.SHP", "SIDEGDI3.SHP",
    "SELL.SHP", "POWER.SHP", "WAYP.SHP", "REPAIR.SHP",
    "R-UP.SHP", "R-DN.SHP",
    "RCLOCK2.SHP", "GCLOCK2.SHP", "DARKEN.SHP",
    "RADAR.SHP", "TABS.SHP", "POWERP.SHP", "MOUSE.SHP",
)

FONTS = ("12METFNT.FNT", "KIA6PT.FNT", "6POINT.FNT", "8POINT.FNT", "GRAD6FNT.FNT", "EDITFNT.FNT")

PALETTE_NAME = "SIDEBAR.PAL"


class BuildError(RuntimeError):
    pass


def manifest(scale: int, shapes, fonts) -> str:
    lines = ["; Written by tools/hdui/build_hdui_pack.py", "", "[UI]", f"Scale={scale}", "", "[Scale]"]
    for name in list(shapes) + list(fonts):
        lines.append(f"{name}={scale}")
    return "\n".join(lines) + "\n"


def build_shape(source: Path, dest: Path, pal: palette_module.Palette, scale: int,
                work: Path, upscale: bool, model: str, gpu: int) -> Path:
    folder = work / source.stem
    shp2png.export(source, folder, pal)

    if upscale:
        upscale_ui.frames(folder, scale, model, gpu)
    else:
        for frame in sorted(folder.glob("frame*.png")):
            image = png.read(frame.read_bytes())
            frame.write_bytes(png.write(upscale_ui.repeat(image, scale)))

    shape = png2shp.build(folder, pal, scale)
    dest.write_bytes(shp.write(shape))
    return dest


def build_font(source: Path, dest: Path, scale: int, work: Path, upscale: bool) -> Path:
    folder = work / source.stem
    fnt2png.export(source, folder)

    sheet = folder / "sheet.png"
    if upscale:
        upscale_ui.pixel_art(sheet, sheet, scale)
    else:
        image = png.read(sheet.read_bytes())
        sheet.write_bytes(png.write(upscale_ui.repeat(image, scale)))

    font = png2fnt.build(folder, scale)
    dest.write_bytes(fnt.write(font))
    return dest


def build(archives, out_dir: Path, scale: int = 2, cameos=(), upscale: bool = True,
          model: str = "realesr-general-x4v3", gpu: int = 0, work_dir: Path | None = None) -> dict:
    out_dir = Path(out_dir)
    if any(part.lower() == "run" for part in out_dir.resolve().parts):
        raise BuildError("the destination is inside Run/, which holds the retail data")
    out_dir.mkdir(parents=True, exist_ok=True)

    names = list(SHAPES) + list(cameos) + list(FONTS) + [PALETTE_NAME]

    with tempfile.TemporaryDirectory() as scratch:
        work = Path(work_dir) if work_dir else Path(scratch)
        raw = work / "raw"
        found = mixextract.extract(archives, names, raw)

        if PALETTE_NAME not in found:
            raise BuildError(f"{PALETTE_NAME} is in none of the archives given")
        pal = palette_module.Palette.load(found[PALETTE_NAME])

        built = {"shapes": [], "fonts": [], "missing": []}

        for name in list(SHAPES) + list(cameos):
            source = found.get(name)
            if source is None:
                built["missing"].append(name)
                continue
            build_shape(source, out_dir / name, pal, scale, work / "shapes", upscale, model, gpu)
            built["shapes"].append(name)

        for name in FONTS:
            source = found.get(name)
            if source is None:
                built["missing"].append(name)
                continue
            build_font(source, out_dir / name, scale, work / "fonts", upscale)
            built["fonts"].append(name)

    (out_dir / "HDPACK.INI").write_text(manifest(scale, built["shapes"], built["fonts"]))
    return built


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("out_dir", type=Path, help="the folder to write the pack into")
    parser.add_argument("--mix", type=Path, action="append", required=True,
                        help="an archive to search, earliest first; repeat for more")
    parser.add_argument("--scale", type=int, default=2)
    parser.add_argument("--cameo", action="append", default=[],
                        help="an extra shape name to include, such as GACNST.SHP")
    parser.add_argument("--model", default="realesr-general-x4v3")
    parser.add_argument("--gpu", type=int, default=0)
    parser.add_argument("--no-upscale", action="store_true",
                        help="repeat pixels instead of running the upscaler, for a dry run")
    parser.add_argument("--work-dir", type=Path, help="keep the intermediate frames here")
    args = parser.parse_args(argv)

    built = build(args.mix, args.out_dir, args.scale, args.cameo, not args.no_upscale,
                  args.model, args.gpu, args.work_dir)

    print(f"{len(built['shapes'])} shapes and {len(built['fonts'])} fonts at {args.scale}x "
          f"in {args.out_dir}")
    if built["missing"]:
        print("not in any archive given: " + ", ".join(built["missing"]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
