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

## Unpack the archives first

An installed game keeps few of its archives loose. `TIBSUN.MIX` carries others
inside it, and the reader here opens a file rather than a member, so the nested
ones are extracted before a pack is built:

```powershell
python mixextract.py raw --mix Run\TIBSUN.MIX --name CONQUER.MIX --name LOCAL.MIX --name CACHE.MIX
python mixextract.py raw --mix Run\SIDECD01.MIX --mix Run\TIBSUN.MIX --mix raw\CONQUER.MIX --name SIDEC01.MIX
```

`SIDEC01.MIX` is the archive to take the sidebar from. The engine mounts two
per-side families -- `SIDECD%02d.MIX` and `SIDEC%02d.MIX` -- and caches only the
second (`code/init.cpp:6355`), which is the one `MFCD::Retrieve` can answer from,
so `SIDEBAR.PAL` and the sidebar shapes come from there. `01` is GDI and `02`
Nod. With Firestorm enabled the engine reads `E%02dSCD%02d.MIX` instead.

`mixextract.py` reports which names it found, so it doubles as the way to ask an
archive what it holds. The index stores a checksum rather than a name, so a name
can be tested but the members cannot be listed.

## Build a pack

```powershell
python build_hdui_pack.py HD --mix raw\SIDEC01.MIX --mix raw\CACHE.MIX --mix raw\LOCAL.MIX --mix Run\TIBSUN.MIX --mix raw\CONQUER.MIX --scale 2
```

The archives are searched in the order given and the first holding a name answers
for it, as the engine mounts them. `SIDEC01.MIX` goes first so the sidebar
artwork and its palette answer before anything else carrying those names; the
fonts come from `CACHE.MIX` or `LOCAL.MIX`. Names it cannot find are listed at
the end and left out of the manifest.

`SIDEGDI1.SHP`, `SIDEGDI2.SHP` and `SIDEGDI3.SHP` are reported missing and should
be. They are a fallback the engine reaches for only when the sidebar shape is
absent (`code/sidebar.cpp:2784`); `SIDE1.SHP` and its two companions are what it
draws.

Each shape is taken to colour and back through the palette the engine draws it with: `MOUSEPAL.PAL` for `MOUSE.SHP`, `CAMEO.PAL` for a cameo, and `SIDEBAR.PAL` for the rest. A shape whose palette is in none of the archives is reported missing rather than built through the wrong one.

A pack built from `SIDEC01.MIX` holds GDI's sidebar. A loose file answers whoever
is playing, so that artwork is drawn for Nod as well.

## Add the cameos

The engine takes each cameo's name from the `Cameo=` entries in `ART.INI`, and
from `ARTFS.INI` when Firestorm is installed, and draws `XXICON.SHP` for a type
with none. `--cameos-from` reads every such entry, so the whole set is built
without naming each shape:

```powershell
python build_hdui_pack.py HD --mix raw\SIDEC01.MIX --mix raw\CACHE.MIX --mix raw\LOCAL.MIX --mix Run\TIBSUN.MIX --mix raw\CONQUER.MIX --cameos-from ART.INI
```

An art file named on the command line is read from disk when it is there and
otherwise taken from the archives. `--cameo GACNSTICON.SHP` adds one cameo by
name.

`ART.INI`, `CAMEO.PAL` and the base game's cameos are in the archives above.
Firestorm keeps `ARTFS.INI`, `ECACHE01.MIX`, `E01SC01.MIX` and `E01SC02.MIX`
inside `EXPAND01.MIX`; the last two hold its side-specific cameos, such as
`MWARICON.SHP` and `LIMPICON.SHP`. Unpack them and list them after
`SIDEC01.MIX`, which must stay first for the sidebar palette:

```powershell
python mixextract.py raw --mix Run\EXPAND01.MIX --name ARTFS.INI --name ECACHE01.MIX --name E01SC01.MIX --name E01SC02.MIX
python build_hdui_pack.py HD --mix raw\SIDEC01.MIX --mix raw\ECACHE01.MIX --mix raw\E01SC01.MIX --mix raw\E01SC02.MIX --mix raw\CACHE.MIX --mix raw\LOCAL.MIX --mix Run\TIBSUN.MIX --mix raw\CONQUER.MIX --cameos-from ART.INI --cameos-from raw\ARTFS.INI
```

With these archives every cameo the two art files name is found: 83 from
`ART.INI` and 8 from `ARTFS.INI`, 89 shapes once shared names are counted once.
`mixextract.py probe` tests whether other names are present:

```powershell
python mixextract.py probe --mix raw\SIDEC01.MIX --mix raw\CACHE.MIX --name CAMEO.PAL --name E1ICON.SHP
```

A cameo the archives lack is listed as missing at the end of the run and the
build goes on. Before the shapes are
built the run prints how many cameos each art file names and how many the
archives hold, then one line per shape. A pack carrying only some cameos
works: the engine enlarges the rest as it draws.

`python artini.py ART.INI` prints the list without building anything.

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
| `artini.py` | Lists the cameos an art file names. |
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
the test writes, the cameo list read from an art file, and a whole pack build
with the upscaler stood down. The
upscaler itself is not covered: it is a separate program.
