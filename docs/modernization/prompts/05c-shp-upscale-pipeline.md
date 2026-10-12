# Prompt 05c — SHP upscale pipeline (units, infantry, buildings, animations)

Context to load: `@docs/modernization/kb/04-assets-and-tactical.md`,
`@docs/modernization/kb/06-upscale-tooling-amd.md`, `@docs/modernization/kb/05-file-resolution.md`.

Depends on 02 and 05b (a 2× SHP dropped into `HD/` is tagged scale 2 and drawn 1:1 at
`AssetScale=2`). Reuses `shp2png.py`/`png2shp.py` from 03b.

## Starting point

- Branch `fork/hd-assets`, created from `fork/voxel-scale` (the 05b engine) with
  `fork/hud-scale` merged in (the `tools/hdui` modules). Neither branch contains the
  other; resolve the merge before planning and say what conflicted.
- `AssetScale=2` still draws objects over buildings they stand against
  (`docs/modernization/REVISIT-05b-depth-sorting.md`). Do not fix it here. Judge art
  from the comparison sheets; play-test items that depend on depth order are recorded
  as blocked by 05b, not as failures.
- Reuse `tools/hdui` (`shp.py`, `palette.py`, `png.py`, `mixextract.py`,
  `upscale_ui.py`, `artini.py`) by import rather than copying. `shp.py` already writes
  0 for a frame whose body exceeds the 16-bit size field; the engine never reads it.
- Lessons from 03b and 03c (`docs/modernization/STATE-03b.md`):
  - Build each shape through the palette the engine draws it with, taken from the
    engine source. 03b broke the cursor by using the sidebar palette for it.
  - Test the output against what the engine reads, from the engine source, not only
    against the pipeline's own reader. The 2× fonts pass every round-trip test and
    still draw as garbage.
  - Many archives are nested and only cached ones answer `MFCD::Retrieve`. Find
    shapes and palettes with `mixextract.py probe`; MIX indexes cannot be listed.
    Known so far: `CONQUER.MIX`, `LOCAL.MIX`, `CACHE.MIX` inside `TIBSUN.MIX`;
    `SIDEC01.MIX` in `SIDECD01.MIX`; `ECACHE01.MIX`, `E01SC01.MIX`, `E01SC02.MIX` and
    `ARTFS.INI` in `EXPAND01.MIX`. Theater archives are not yet located.
  - The user's `realesrgan-ncnn-vulkan` has 2× weights only for `realesr-animevideov3`.
    List the models folder before naming another; a 4× model scaled down to 2× is the
    alternative worth evaluating on the proof set.
  - Print `[i/n] NAME` progress per shape and per GPU batch, flushed.
- The user's setup and build commands are in `STATE-03b.md`. Present design choices
  as options with a recommendation.

## Goal

`tools/hdassets/` — a pipeline that turns the game's 8-bit SHP sprites into 2× SHPs on
the **same palettes**, preserving transparency, house-colour remap indices, shadow
frames, frame offsets and frame order, using Real-ESRGAN on the Radeon via ncnn-vulkan.
Start with a curated proof set, then batch by category.

## Verify first

- `code/shapeset.h` layout and `code/rle.cpp`; the `Get_Count()/2` shadow-frame
  convention in `techno.cpp`; which SHP categories use which palette (`UNIT<theater>.PAL`
  for units/buildings/infantry with remap 16–31; `ANIM.PAL` for animations;
  `TEMPERAT/SNOW/URBAN` iso palettes for terrain objects; `Cameo` palette) — locate the
  palette choice per `ObjectTypeClass` subtype in the loaders and record it in a table.
- Which SHPs carry Z-shapes (`z_shapefile` args to `Draw_Shape`) or alpha (`SHAPE_ALPHA`
  images like `alphashp`) — those need nearest upscaling, not ESRGAN.
- Buildings: `BuildingTypeClass` image + `_A` active anims + damaged frames + bibs; the
  `Foundation` is in cells, unaffected.
Show the plan and the proof set.

## Build

1. `catalog.py` — enumerate every SHP name the engine can request from `art.ini`,
   `rules.ini` and hard-coded lists (grep `MFCD::Retrieve`/`Fetch_Shape` literals), with
   category, palette, theater suffix variants, and whether it has shadow frames (frame
   count even and second half 1-bit — detect by decoding). Output `catalog.json`.
2. `shp_upscale.py` — per SHP:
   - decode → RGB frame (through its palette) + alpha mask + remap mask + shadow frames
     as masks;
   - RGB → `realesrgan-ncnn-vulkan -s 2` (`realesr-general-x4v3` then 2× downsample, or
     x4plus/2 — make the model a flag; evaluate both on the proof set);
   - masks → nearest 2× (or thresholded bilinear for alpha; flag);
   - quantise: non-remap pixels → nearest palette index in CIELAB over the allowed
     range (exclude 0 and 16–31 for unit palette; exclude the theater-specific reserved
     ranges you find); remap pixels → nearest among 16–31 by L*; optional tiny Floyd–
     Steinberg (flag, default off);
   - encode 2× SHP with doubled `X, Y, Width, Height`, same flags, RLE if source used
     it, `Color[3]` copied; shadow frames from the 2× masks.
   - write `<name>.SHP` into `HD/` (theater variants keep their suffix).
   Resumable; a manifest of md5s.
3. `verify_shp.py` — re-decode output, assert frame count, offset doubling, palette
   membership, shadow frames 1-bit, and render frame 0 side by side (1× nearest-doubled
   vs upscaled) into `work/compare/<name>.png`.
4. `run_proof.py` — proof set (SHP subjects only; vehicles are voxels, covered by 05a): `E1`, `CYBORG`, `GACNST` + `GACNST_A`, `GAPOWR`,
   `NAHAND`, `TWLT026` (tree), `TIB01`+ tiberium overlays, `EXPLOLRG`, `RING1` (ion
   effect). Produce the comparison sheet and stop.
5. `run_batch.py --category infantry|buildings|anims|overlays|terrainobjects` for the
   batch, per category, with timings.
6. `README.md`: install, GPU flags, palette table, what not to upscale (Z-shapes, alpha
   lights, palette-cycling effects), and known artefacts.

Tests on synthetic SHPs only (`tools/hdassets/tests/`).

## Deliverables

Tools, proof comparison sheet, README, a short `manual/content/systems/hd-assets.md`
update describing the pack layout, and a play-test checklist for the proof set at
`AssetScale=2`: infantry remap shows house colour, shadows align, building active anims
align on the building, damaged frames, explosion anims centred, tree shadows, tiberium
overlays tile without seams, cameo unaffected, no palette-cycling breakage (ion storm,
water).
