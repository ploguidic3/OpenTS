# Play-test checklist — 02 Asset overrides

Build: `fork/asset-overrides`. Needs a retail install under `Run/` and one edited SHP. Make the
edit with any SHP editor (XCC Mixer or OS SHP Builder): extract `SIDE1.SHP` from `CONQUER.MIX`,
recolour a strip of it, and save it under the same name. Run the Debug build with `-WIN` so the
debug log is readable.

## Loose file wins
- [ ] Create `Run\HD\` and copy the edited `SIDE1.SHP` into it.
- [ ] Write `Run\OPENTS.INI` with `[Paths]` `SearchPaths=HD,INI,MIX,Maps` (see `docs/modernization/examples/OPENTS.INI`).
- [ ] Start a skirmish → the sidebar shows the edited art.
- [ ] Debug log holds a line like `[ShapeLoad] SIDE1.SHP from ...HD\SIDE1.SHP, N bytes, scale 1, N loose bytes held`.
- [ ] Options → the game plays otherwise unchanged: cursor, tabs, radar, cameos, units and buildings look stock.

## Search order
- [ ] Copy a differently edited `SIDE1.SHP` beside `Game.exe` (the game's own directory) → that copy wins over `HD\`.
- [ ] Remove it again → the `HD\` copy is back.

## The switch
- [ ] Set `AssetOverrides=no` under `[Video]` in `SUN.INI` and restart → stock sidebar art, no `[ShapeLoad]` line, log says `AssetOverrides are OFF`.
- [ ] Set it back to `yes` (or delete the line) → edited art returns.
- [ ] Options → save settings from the in-game options dialog → `SUN.INI` still carries `AssetOverrides=yes`.

## Scale tag (no visual change expected yet)
- [ ] Put a `SIDE1.SHP` whose header width and height are exactly double the stock one's into `HD\` → the log line says `scale 2`. It is drawn as-is (larger, clipped); that is expected until prompt 03b.
- [ ] Add `HD\HDPACK.INI` with `[Scale]` `SIDE1.SHP=1` → the log line says `scale 1`.
- [ ] A stock-size `SIDE1.SHP` with `SIDE1.SHP=2` in `HDPACK.INI` → the log line says `scale 2`.

## Bad file
- [ ] Put a 3-byte text file named `RADAR.SHP` into `HD\` → the game starts, the radar frame is the stock art, the log says the file is too short and the archived copy is used.

## Theater change
- [ ] Play a temperate mission, then a snow mission in the same session → object art follows the theater as before (theater-suffixed names are fetched through the same path).

## Not affected (spot check)
- [ ] Fonts, palettes, sounds, music, movies and INI files load as before.
- [ ] A loose `RULES.INI` or `.PCX` title art in `HD\` still overrides as it did before this change.
