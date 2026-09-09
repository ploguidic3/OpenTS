# Prompt 03b — HD interface art and fonts

Context to load: `@docs/modernization/kb/00-project-overview.md`,
`@docs/modernization/kb/02-rendering-and-ui.md`, `@docs/modernization/kb/05-file-resolution.md`,
`@docs/modernization/kb/06-upscale-tooling-amd.md`.

Depends on 02 (loose SHP overrides, `Shape_Scale`) and 03a (`UIScale`).

## Goal

Replace the nearest-neighbour magnification from 03a with real 2× interface art and 2×
bitmap fonts when a pack provides them, and produce that pack with an upscale pipeline.
With a 2× pack at `UIScale=2` the HUD renders 1:1; at `UIScale=3`/`4` the engine
magnifies the 2× art by the remaining factor.

## Verify first (plan mode)

- `Fetch_Shape` from prompt 02 returns a scale tag; `ShapeButtonClass::Set_Shape` sizes
  from the SHP; `SidebarClass::Draw_It` stacks `SIDE1/2/3/ADDON`; `Max_Visible` divides
  by `SidebarMiddleShape->Get_Height()`.
- `WWFontClass` header struct (`code/wwfont.h`) and `Print` (`code/wwfont.cpp`) are the
  only `.FNT` consumers; fonts are loaded from mix data in `code/init.cpp` via
  `Load_Alloc_Data`/`MFCD::Retrieve`.
- `MSFont` glyphs are SHP frames (graphic menu only — out of scope).
Show me the plan.

## Engine work

1. HUD surfaces at pack scale. When every HUD shape the sidebar needs reports
   `Shape_Scale()==2` (or a pack manifest `HDPACK.INI [UI] Scale=2` says so), allocate
   `SidebarSurface`/`TabSurface` at 2× and set an internal `HudArtScale=2`; the composite
   stretch factor becomes `UIScale / HudArtScale` (1 at 1440p). All layout constants from
   03a multiply by `HudArtScale` at draw time and by `UIScale` for hit tests — keep the
   single multiplication point you built in 03a. Mixed packs (some shapes 1×) fall back to
   `HudArtScale=1` for the whole HUD with a one-line log naming the missing shapes; do not
   try to mix scales inside one surface.
2. Fonts: add `.FNT` scale support — a loose `12METFNT.FNT` (etc.) in the `HD` folder
   with a `Height` twice the mix version is tagged scale 2 the same way shapes are. Give
   `FontClass` a `Scale()` accessor; `Fancy_Text_Print`/`Draw_Text_Scaled` from 03a use
   the 2× font at 1:1 when `HudArtScale==2`. Cover `12METFNT, KIA6PT, 6POINT, 8POINT,
   GRAD6FNT, EDITFNT`.
3. Cursor: `MOUSE.SHP` at 2× through the same override; `Build_Cursor` already scales
   from the shape size — make `Cursor_Scale()` account for a 2× shape (halve the
   multiplier).
4. Cameos: `XXICON.SHP` per unit; the override applies per file, so a pack can be
   partial here — allow mixed 1×/2× cameos by drawing a 1× cameo through the 2× nearest
   path inside the strip (the one place mixed scale is worth supporting).

## Pipeline work (`tools/hdui/`, Python 3.12, runs on the RX 9070 XT via ncnn-vulkan)

1. `mixextract.py` — read a `.MIX` (TS format with the encrypted header variant; the
   engine's `code/mixfile.cpp` and `code/blowfish.cpp`/`pk.cpp` are the reference) and
   extract by name from a supplied name list. If this is more than an afternoon, fall
   back to documenting XCC Mixer for extraction and skip the reader.
2. `shp2png.py` / `png2shp.py` — decode/encode TS SHP per `code/shapeset.h` (RLE per
   `code/rle.cpp`), emitting per-frame PNG + alpha mask + metadata JSON (offsets, flags,
   `Color[3]`); encode the reverse at 2×.
3. `fnt2png.py` / `png2fnt.py` — per `WWFontClass`'s header/blocks; glyph sheets out,
   2× glyph sheets in.
4. `upscale_ui.py` — for each HUD asset: PNG → `realesrgan-ncnn-vulkan -s 2` (use
   `realesr-general-x4v3` then downsample, or x4plus/2) → quantise to `SIDEBAR.PAL`
   (nearest in Lab, no dither) → 2× SHP. Masks upscaled nearest. Fonts: upscale glyph
   sheets with a pixel-art-aware method (xBRZ or hq2x — `pip install pyxbr` or a small
   xBRZ port; ESRGAN smears 6-pixel type), threshold back to 1-bit.
5. `build_hdui_pack.py` — orchestrates for the list in kb 02 §4 plus fonts and cursor,
   writes into `HD/` and an `HDPACK.INI`.
6. A `README.md` in `tools/hdui/` with the exact commands and the GPU flags.

Keep the tools free of retail data: test them on synthetic SHP/FNT bytes generated in
`tools/hdui/tests/`.

## Deliverables

Engine code (Debug+Release), tools, `manual/changes/hd-interface-art.md`,
`manual/content/formats/hdpack-ini.md`, updates to `shp.md`/`ui-scale` pages, and a
play-test checklist: run the pack at 1440p (`UIScale` auto → 2, expect 1:1 HD art), at
1080p (`UIScale` 2 → same), at 4K (`UIScale` 3 → 2× art magnified 1.5× — confirm you
accept the look or prefer 2 or 4), check cameo mixed-scale path, cursor size, fonts in
chat and tooltips, `AssetOverrides=no` returns to 03a behaviour.
