# Prompt 02 — Asset override infrastructure

Context to load: `@docs/modernization/kb/00-project-overview.md`,
`@docs/modernization/kb/05-file-resolution.md`, `@docs/modernization/kb/04-assets-and-tactical.md`.

## Goal

Make every SHP the engine loads overridable by a loose file, so HD art can be developed
in a folder without repacking MIX archives. Add the `HD` search folder convention and a
scale tag on loaded shapes that later prompts consume. No visual change on its own.

## Verify first (plan mode)

- `MFCD::Retrieve` (`code/mixfile.cpp`) returns a pointer into cached mix memory and has
  no filesystem fallback.
- `ObjectTypeClass::Fetch_Normal_Image` (`code/objtype.cpp`) and the HUD loaders in
  `sidebar.cpp`, `radar.cpp`, `tab.cpp`, `power.cpp`, `mouse.cpp`, `display.cpp`,
  `techtype.cpp` all call `MFCD::Retrieve` for `.SHP`.
- `MouseClass::One_Time` (`code/mouse.cpp`) has the `_DEBUG`-only loose-file pattern.
- `CDFileClass::Set_Name` search order and `Init_Search_Folders` /
  `DeploymentConfigClass::SearchPaths` behave as the kb describes.
- Count the `MFCD::Retrieve` call sites and classify them: SHP, PAL, FNT, VPL, other.
Show me the list and the plan.

## Design

1. New files `code/shapeload.h/.cpp` (fork header, "OpenTS contributors"):
   ```cpp
   struct ShapeSource { ShapeSet const * Shape; int Scale; bool Owned; };
   ShapeSet const * Fetch_Shape(char const * name);          // drop-in for Retrieve on SHP
   ShapeSource const * Fetch_Shape_Source(char const * name); // with scale metadata
   int Shape_Scale(ShapeSet const * shape);                   // 1 if unknown
   ```
   `Fetch_Shape` tries, in order: a loose file named `<name>` through `CCFileClass`
   semantics (which already walks user path → cwd → search folders); then
   `MFCD::Retrieve`. Loose loads use `Load_Alloc_Data` and are kept in a process-lifetime
   registry keyed by upper-cased name (mixes are never freed either; document this).
   A loose SHP whose logical `Width/Height` are exactly 2× the mix version's (when a mix
   version exists) is tagged `Scale = 2`; otherwise a sidecar convention decides:
   `<name>.SHP` alongside `<name>.scale` containing `2`, or a `[Scale]` section in an
   `HDPACK.INI` in the same folder. Pick the sidecar-free heuristic plus the INI override;
   keep it simple and documented.
2. Replace `MFCD::Retrieve` with `Fetch_Shape` at every SHP call site. Leave PAL/FNT/
   VPL/other sites alone in this prompt (fonts come in 03b). Keep each replacement a
   one-line change; do not restructure callers.
3. `OPENTS.INI`: no new key. Document the convention `SearchPaths=HD,INI,MIX,Maps` and
   ship a commented example `OPENTS.INI` under `docs/modernization/examples/`. Add an
   `HD/` entry to `.gitignore` so a development pack in the checkout is never committed.
4. `SUN.INI [Video]`: `AssetOverrides=yes|no` (default yes) so a player can disable loose
   overrides without moving files. Read next to `CursorScale`.
5. Memory: loose loads live in the 32-bit heap alongside cached mixes. Log total loose
   bytes at startup through the existing debug print path.

## Tests

Extend `tests/gamedirs` (or add `tests/shapeload`) with a harness that: creates a temp
search folder, writes a synthetic 2-frame SHP with the layout from `code/shapeset.h`,
points the search chain at it, and checks `Fetch_Shape` returns the loose bytes, tags
scale 2 when a synthetic 1× "mix" version is registered through a stub, and returns the
stub's pointer when the loose file is absent or `AssetOverrides=no`. No retail data.

## Deliverables

Code; Debug+Release builds; CTest green; `manual/changes/loose-shape-overrides.md`
(feature, `release: fork`); `manual/content/keys/assetoverrides.md` + `ini-keys.yaml`;
correct the overstatement in `manual/content/formats/opents-ini.md` and the sentence in
`manual/content/formats/shp.md` about archive-only fetching; a play-test checklist
(drop an edited `SIDE1.SHP` into `HD/` and see it, then set `AssetOverrides=no`).
