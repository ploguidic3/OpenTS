# Prompt 05b — `AssetScale=2` engine mode

Context to load: `@docs/modernization/kb/00-project-overview.md`,
`@docs/modernization/kb/04-assets-and-tactical.md`, `@docs/modernization/kb/05-file-resolution.md`.

Depends on 02 (`Fetch_Shape`, `Shape_Scale`) and 05a (`VoxelScale`).

## Goal

A `[Video] AssetScale=1|2` mode in which the tactical view's world is projected at twice
the pixel density (96×48 cells), HD (2×-tagged) shapes draw 1:1, and every legacy 1×
shape draws through a 2× nearest-neighbour blitter. The game must be fully playable at
`AssetScale=2` with **no** HD art installed — every sprite simply looks like today's
frame magnified 2× — so a pack can be built incrementally. Terrain tiles are excluded
here (05d): in this prompt they are drawn through the 2× nearest path too.

## Verify first (plan mode)

Re-grep every entry in kb 04 §7 "Blockers". In addition, grep for:
- `ISO_TILE_PIXEL_W`, `ISO_TILE_PIXEL_H`, `LEVEL_PIXEL_H`, `LEPTON_TO_PIXEL`,
  `PIXEL_TO_LEPTON`, `CELL_PIXEL_`, literal `576`, literal `48`/`24`/`12` in
  `tactical.cpp`, `display.cpp`, `cell.cpp`, `isotype.cpp`, `radar.cpp` (radar must
  **not** scale), `scroll.cpp`, `foot.cpp`, `techno.cpp` (selection brackets, health
  bars, pips, veterancy chevrons), `building.cpp` (rubble/roof offsets), `anim.cpp`,
  `bullet.cpp`, `laser.cpp`, `wave.cpp`, `ion.cpp`, `particle.cpp`, `partsys.cpp`.
- `Draw_Shape` and `Blit_Block` callers that draw *UI* things into the tactical surface
  (placement grid, waypoint flags, action lines, rubber band `Draw_Rubber_Band`, chat).
Produce a categorised inventory (world geometry / sprite placement nudge / HUD-in-
tactical / radar) with counts, and the plan. This inventory is the deliverable of the
planning step; expect it to be long.

## Design

1. `AssetScale` global in a fork header `code/assetscale.h`: `int Asset_Scale()`,
   `inline int AS(int px) { return px * Asset_Scale(); }`. Read from `[Video]`; fixed
   for the process (changing it needs a restart — document; do not attempt live
   switching because surfaces, caches and the tile tables all depend on it).
2. Geometry: `ISO_TILE_PIXEL_W/H`, `LEVEL_PIXEL_H` become functions or are initialised
   from `Asset_Scale()` at startup (they are `const int` in `globals.cpp` — convert to
   plain ints set in one init function). Replace `LEPTON_TO_PIXEL`/`PIXEL_TO_LEPTON`
   with scale-aware inline functions (`/7` becomes `* Asset_Scale() / 7`); replace `576`
   with an expression of the tile constants; remove the `static` from
   `Z_Lepton_To_Pixel`'s locals or compute them at init; rebuild `PixelToCoordMatrix`
   with the scale. Scroll steps, edge scroll, `Set_Tactical_Position`, `Pixel_To_Cell`
   must round-trip: add a unit test in a new `tests/tacticalmath` harness (compile the
   projection helpers alone) checking `Cell → pixel → Cell` identity at scale 1 and 2.
3. Iso-tile rasteriser: regenerate the diamond mask and span tables from `ISO_WIDTH*scale`
   at init instead of the literal tables; for 1× TMP data at scale 2, expand the tile
   image 2× nearest into a scratch buffer before the existing draw (Z-data expanded the
   same way). This is the interim terrain path.
4. `Draw_Shape`: add an internal scaled variant used when `Shape_Scale(shape) <
   Asset_Scale()`. Implement one new blitter family "nearest 2×" for the flag
   combinations actually used by tactical draws (transparent, remap, darken/shadow,
   translucent 25/50/75, z-read/z-write, alpha) — measure which combinations occur via a
   debug counter before writing them; skip the rest and assert. Simplest correct
   approach: expand the 8-bit frame 2× into a per-frame scratch `BSurface` (cached per
   `(shape, frame)` in a small LRU keyed by pointer+index — memory bound, say 64 MB) and
   feed the existing blitters. That avoids touching `blitblit.h` at all. Prefer it.
5. `Techno_Draw_Object`: honour its `scale` argument at last, or delete the parameter —
   decide and be consistent.
6. Sprite placement nudges: every `Point2D(literal, literal)` offset in `Draw_It` bodies
   and health-bar/pip/bracket code is multiplied via `AS()`. Selection bracket sizes
   (`PixelSelectionBracketDelta` in art.ini is already pixels — scale on read).
7. HUD-in-tactical drawings (rubber band, placement grid, waypoint labels, action lines)
   are 1-px lines: keep them 1 px, they are UI, but scale their positions.
8. Radar: untouched — it composes from cell colours, not pixels. Verify.
9. Voxels: `VoxelScale = Asset_Scale()` (05a); the region blit lands at the scaled
   projection.
10. Memory and performance: the scratch cache plus 2× voxel arena plus 4K 565 surfaces.
    Log peak working set at scenario load. Tactical `Render` cost roughly quadruples per
    cell; measure fps at 4K on a busy map and report. If the pan optimisation is broken
    by any change (ring buffers), fix rather than disable.
11. Saves, replays, network: leptons are unchanged, so the simulation is unaffected. Put
    that in the change record with the evidence (no `Coord`/`Cell` code touched — verify
    with a grep of your diff).

## Tests

`tests/tacticalmath` (projection round-trip, both scales), a `tests/scaleblit` harness
(2× nearest expand of a synthetic 8-bit frame: dimensions, index preservation, RLE
transparent index 0 stays 0), CTest green including `voxeldraw`.

## Deliverables

Code; Debug+Release; CTest; `manual/changes/asset-scale-mode.md` (feature, with the
"restart required" note); `manual/content/keys/assetscale.md` + `ini-keys.yaml`;
`manual/content/systems/` page "HD asset scale" describing the fallback and the
`Shape_Scale` tagging; play-test checklist at 1440p and 4K with `AssetScale=2` and no
pack: units select/move/attack correctly (hit tests), buildings place on the right cells,
cliffs and ramps line up, bridges, tunnels, shroud/fog edges, ion storm, veins, tiberium
growth, harvester docking, infantry sub-cell positions, aircraft altitude offsets,
projectile trajectories, laser/wave/EMP effects, cloak shimmer, building damage/ rubble
frames, the map editor if compiled, save/load in both scales (a save made at scale 1 must
load at scale 2 — it should, leptons only), and frame rate.
