# Prompt 05d — Terrain tiles (TMP) at 2×

Context to load: `@docs/modernization/kb/04-assets-and-tactical.md`,
`@docs/modernization/kb/06-upscale-tooling-amd.md`, `@docs/modernization/kb/05-file-resolution.md`.

Depends on 05b (tile rasteriser parameterised, interim nearest path) and 05c (quantiser
and ncnn pipeline).

## Goal

Real 2× isometric terrain: `.TEM/.SNO/.URB` tile sets upscaled to a 96×48 diamond with
coherent Z-data and extra images, loaded through the existing `CCFileClass` path (loose
tiles already override) and tagged scale 2 so the rasteriser draws them 1:1.

## Verify first

- `IsoTileRecord`/`IsoTileSet` in `code/isotype.h`: per-cell image, optional extra
  image (with its own X/Y/W/H), optional Z data and extra Z data, `Height`, `RampType`,
  `LowColor/HighColor`.
- `IsometricTileTypeClass::Load_Tile_Data`, discovery and theater suffix handling; the
  05b init-time mask/span tables; how Z data feeds `ZBuffer` writes (`Render_Tiles`,
  `Render_Tile_Shadows`) and cliff/ramp height maths (`LEVEL_PIXEL_H`).
- `manual/content/formats/theater-control.md` and the theater INI.
Show the plan; include how a 2× tile is detected (`Width/Height` = 96/48 in the header).

## Engine

1. Tile scale tag: in `Load_Tile_Data`, if the set's `Width/Height` are `2×ISO_WIDTH/
   ISO_HEIGHT` at `AssetScale=2`, use it directly; if 1×, use the 05b expand path. Mixed
   sets per theater are fine (per-file).
2. Z data: at 2× the Z plane is 96×48 bytes per cell; the depth values themselves are
   in the same units as before (levels), so no remapping — verify by reading
   `Render_Tiles`'s Z write.
3. Extra images (cliff overhangs) offsets doubled.
4. Radar/minimap uses `LowColor/HighColor`, unchanged.

## Tools (`tools/hdassets/tmp_upscale.py`)

Per tile set: decode every cell's image + extra image (through `ISO<theater>.PAL`) with
the diamond mask as alpha; upscale RGB with `realesr-general-x4v3 -s 2` (evaluate a
seam-aware approach: upscale the whole tile set's cells assembled into their map layout
`MapWidth×MapHeight` as one image so edges between cells stay continuous, then cut back
into cells); quantise to the iso palette (exclude reserved/animated ranges — check the
palette for cycling water indices); Z planes and extra-Z nearest 2×; write a 2× TMP with
doubled dimensions and offsets. Verify by re-decoding and rendering a 3×3 block through a
harness that compiles `isotype.cpp`'s decode path against synthetic tiles.

Proof set: `TEMPERAT` clear tiles, one cliff set, one ramp set, a shore/water set, a
pavement set. Compare sheet, then stop for judgement before the batch over all three
theaters.

## Deliverables

Engine code (Debug+Release, CTest with a synthetic 2× TMP test), tool, README section,
`manual/changes/hd-terrain.md`, `manual/content/formats/` note on 2× tile sets, play-
test checklist: cliff faces and ramps align with unit movement heights, shore transitions,
bridges (their own SHP overlays), tunnels, ice (snow theater), urban pavement seams, LAT
transitions, tiberium on 2× tiles, veinholes, and the map editor if present.
