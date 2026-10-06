# Prompt 03c — HD cameos

Context to load: `@docs/modernization/STATE-03b.md`,
`@docs/modernization/kb/02-rendering-and-ui.md`, `@docs/modernization/kb/05-file-resolution.md`,
`@docs/modernization/kb/06-upscale-tooling-amd.md`.

Work on branch `fork/hud-scale`. Depends on 03b, whose pack covers the sidebar frame,
buttons, clocks, radar frame, tabs and cursor but no cameos.

## Goal

Add the build cameos to the interface pack so the largest area of the sidebar is upscaled
art rather than classic art enlarged by the engine. A pack with only some cameos keeps
working: the missing ones draw enlarged, as now.

## Verify first (plan mode)

- A cameo's file name is `art.ini [<graphic name>] Cameo=`, falling back to the type's
  own section and then to `XXICON`, plus `.SHP` (`code/techtype.cpp:923-928`; a second
  path at `:689-695`). It is loaded with `Fetch_Shape`, so a loose file in `HD\` replaces
  the archived one.
- Cameos are drawn through `CameoDrawer` (`code/sidebar.cpp:2049`), built from `CAMEO.PAL`
  (`code/init.cpp:2675`). `build_hdui_pack.palette_for(name, cameo=True)` already returns
  `CAMEO.PAL`, and `--cameo NAME.SHP` already adds one cameo.
- The engine already draws a 1× cameo inside a 2× strip (03b item 4, `UI_Art_Shape`).
- Which archives hold the cameo shapes, `CAMEO.PAL` and `art.ini` is unknown. Firestorm
  adds its own art file and cameos. Find them by probing with `tools/hdui/mixextract.py`;
  a MIX index stores checksums, so a name can be tested but members cannot be listed.

Show me the plan before editing. No engine change is expected; if one turns out to be
needed, stop and say why first.

## Work (`tools/hdui`)

1. Read the cameo names out of `art.ini` (and the Firestorm art file when it is given):
   every `Cameo=` value, with `XXICON`. Add an option such as `--cameos-from ART.INI` so a
   whole set is built without naming each one. Names the archives lack are reported as
   missing, not fatal.
2. Build cameos through `CAMEO.PAL`, with the existing per-shape palette routing.
3. Print progress per asset, as `CLAUDE.local.md` requires; a cameo set is hundreds of
   shapes and runs on the user's GPU.
4. Update `tools/hdui/README.md` with the archives and the command, and add the cameo
   checks to `docs/modernization/PLAYTEST-03b-hd-ui-art.md` under Mixed pack.

Tests use synthetic `art.ini` text, shapes and palettes only.

## Report

The exact build command for the user's setup, how many cameos it found and built, what
was not run, and the play-test steps: cameos look upscaled rather than blocky, the clock
and darkening still cover them exactly, and a cameo removed from the pack draws enlarged
in its place.

Follow-ups, not part of this prompt: the visible gain may stay modest with
`realesr-animevideov3`. A model run at 4× and scaled down to 2× is the next thing to try.
