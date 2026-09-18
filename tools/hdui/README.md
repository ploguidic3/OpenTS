# Interface artwork pipeline

Builds a folder of interface artwork at twice the size the game ships, which the
engine draws at one screen pixel per artwork pixel. [HDPACK.INI](../../manual/content/formats/hdpack-ini.md)
describes what the engine does with the result.

Nothing here writes into `Run/`; a destination inside it is refused. The modules
are Python 3.12 and use the standard library only, so the codecs run anywhere;
the upscaler is a separate program and needs a GPU.

## Install

`realesrgan-ncnn-vulkan` from the `xinntao/Real-ESRGAN` releases, which runs on
any Vulkan GPU, including a Radeon. Put it on `PATH` or set `OPENTS_REALESRGAN`
to its full path. Check the GPU is seen:

```powershell
realesrgan-ncnn-vulkan -h        # lists the models and the -g device numbers
```

Glyph sheets want a pixel-art enlarger rather than a neural one, since ESRGAN
smears six-pixel type. Name one with `OPENTS_XBRZ`; it is called as
`<program> -s <scale> <in.png> <out.png>`. Without one the glyph pixels are
repeated, which stays readable and exactly on the grid.

## Build a pack

```powershell
python build_hdui_pack.py HD --mix Run\TIBSUN.MIX --mix Run\CONQUER.MIX --scale 2
```

The archives are searched in the order given and the first holding a name answers
for it, as the engine mounts them. Names it cannot find are listed at the end and
left out of the manifest. `--cameo GACNST.SHP` adds a cameo; the engine takes a
pack that carries only some of them and enlarges the rest as it draws.

Useful flags:

- `--no-upscale` repeats pixels instead of running the GPU, which is the way to
  check the plumbing quickly.
- `--work-dir frames` keeps the intermediate PNGs, so a frame can be redrawn by
  hand and the encoder run again over the folder.
- `--model realesrgan-x4plus` for the heavier model; `--gpu 1` for the second
  device.

Then point the game at the folder, with `SearchPaths=HD,INI,MIX,Maps` under
`[Paths]` in `OPENTS.INI`, and leave `AssetOverrides` on.

## Run one stage

```powershell
python mixextract.py raw --mix Run\TIBSUN.MIX --name SIDE1.SHP --name SIDEBAR.PAL
python shp2png.py raw\SIDE1.SHP frames\SIDE1 --palette raw\SIDEBAR.PAL
python upscale_ui.py frames\SIDE1 --scale 2 --model realesr-general-x4v3 --gpu 0
python png2shp.py frames\SIDE1 HD\SIDE1.SHP --palette raw\SIDEBAR.PAL --scale 2

python fnt2png.py raw\8POINT.FNT sheets\8POINT
python upscale_ui.py sheets\8POINT --scale 2
python png2fnt.py sheets\8POINT HD\8POINT.FNT --scale 2
```

`shp2png.py` writes `frameNNNN.png`, `frameNNNN.mask.png` and `shape.json`. The
mask says which pixels are transparent and is enlarged by repetition, never by
the upscaler, so the transparent border stays crisp. `png2shp.py` quantises each
frame back to the palette, nearest colour in CIE L\*a\*b\* with no dithering, and
multiplies every offset by the scale.

`fnt2png.py` writes `sheet.png`, sixteen glyphs per row in cells of the font's
widest and tallest glyph, with each glyph's palette index as its pixel value, and
`font.json` with the metrics. `png2fnt.py` snaps the enlarged sheet back to those
indices, since a font carries only sixteen.

## What the modules are

| File | Does |
| --- | --- |
| `mixreader.py`, `blowfish.py`, `mixcrypt.py` | Reads MIX archives, encrypted index included. Copied from `tools/cutscenes`; the two copies are identical and can be merged when both land. |
| `mixextract.py` | Pulls named members out of archives. |
| `png.py` | Eight-bit PNG, no interlacing. |
| `shp.py`, `fnt.py` | The SHP and FNT formats, read, write and nearest-neighbour enlargement. |
| `palette.py` | `.PAL` loading and nearest-colour matching in L\*a\*b\*. |
| `shp2png.py`, `png2shp.py`, `fnt2png.py`, `png2fnt.py` | The stages above. |
| `upscale_ui.py` | Drives the upscalers, and repeats pixels where none is installed. |
| `build_hdui_pack.py` | Runs the whole thing and writes `HDPACK.INI`. |

## Tests

```powershell
python -m unittest discover -s tests
```

Every input is synthesised, so the tests need no game data and no GPU. They cover
the codecs, the palette match, both sheet round trips, extraction from an archive
the test writes, and a whole pack build with the upscaler stood down. The
upscaler itself is not covered: it is a separate program.
