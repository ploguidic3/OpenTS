# Play-test checklist — 03b HD interface art

Build: `fork/hud-scale`. Needs a retail install under `Run/` and a 2× artwork folder built
with `tools/hdui`. Run the Debug build with `-WIN` so the debug log is readable.

Most of an installed game's archives sit inside `TIBSUN.MIX` rather than beside it, and the
sidebar's palette is in the cached per-side archive rather than the one that shares its
name. `tools/hdui/README.md` gives the extraction the pack build needs first; a build that
reports no fonts, or stops on `SIDEBAR.PAL is in none of the archives given`, has been
handed too few archives.

Put the pack in a folder the game searches, with `SearchPaths=HD,INI,MIX,Maps` under
`[Paths]` in `OPENTS.INI`, and `HDPACK.INI` in that folder carrying:

```ini
[UI]
Scale=2
```

Log lines to watch for, all at start:

- `[ShapeLoad] HD pack declares interface scale 2`
- `UIScale <setting> resolves to <N> at <W>x<H>, artwork at <A>`
- `[ShapeLoad] <name> from <path>, <size> bytes, scale 2, ...` for each replaced shape
- `[FontLoad] <name> from <path>, <size> bytes, scale 2, ...` for each replaced font
- `[Sidebar] the pack draws at 2 but <names> are the classic artwork, enlarged as drawn`,
  which should not appear for a complete pack
- `[ShapeHD] a <W>x<H> shape could not be enlarged by <f>` — a classic backdrop shape too
  large to enlarge; it will show as a gap

Expected resolution of the scale, with a complete 2× pack:

| Resolution | Setting | Expected N | Expected A | Sidebar surface | On screen |
| --- | --- | --- | --- | --- | --- |
| 1920×1080 | omitted | 2 | 2 | 336 × 1080 | artwork 1:1 |
| 2560×1440 | omitted | 2 | 2 | 336 × 1440 | artwork 1:1 |
| 3840×2160 | omitted | 2 | 2 | 336 × 2160 | artwork 1:1, sidebar 336 px wide |
| 3840×2160 | `UIScale=4` | 4 | 2 | 336 × 1080 | artwork magnified twice, sidebar 672 px |
| 3840×2160 | `UIScale=3` | 2 | 2 | 336 × 2160 | rounded down; log shows 2 |
| 3840×2160 | `UIScale=1` | 1 | 1 | 168 × 2160 | classic artwork, 03a behaviour |
| 1280×720 | omitted | 1 | 1 | 168 × 720 | classic artwork |

## Artwork
- [ ] At 1440p with the pack, the sidebar, tab strip, radar frame, power pips, mode buttons and scroll arrows are smooth HD art, not blocky magnification. Compare a screenshot against the same scene without the pack.
- [ ] The sidebar backdrop has no seam or gap where `SIDE1`, `SIDE2`, `SIDE3` and `ADDON` meet, and reaches the bottom of the screen.
- [ ] The number of cameo rows matches what the magnified 03a build showed at the same resolution.
- [ ] At 4K with `UIScale=4` the HD art is magnified exactly twice, with square pixels and no smoothing.
- [ ] `AssetOverrides=no` in `[Video]` returns to the 03a look at every resolution above, with no other difference.

## Mixed pack
- [ ] Remove one cameo (`XXICON.SHP` for a unit) from the pack. It draws at the right size in the strip, enlarged and blocky, with the strip layout unchanged.
- [ ] Remove `RCLOCK2.SHP`. The production clock still covers the cameo exactly and still blends rather than painting over it.
- [ ] Remove `DARKEN.SHP`. An unavailable cameo is still darkened over its whole picture.
- [ ] Remove `SIDE2.SHP`. The log names it; note what the sidebar looks like and whether you want that case to fall back to the classic HUD instead.

## Hit tests
- [ ] Repair, sell, power and waypoint press where they draw, at their outermost pixels, at every row of the table.
- [ ] A cameo starts production when clicked anywhere inside its picture, including the top and bottom cameo of both columns; right-click cancels it.
- [ ] The scroll arrows scroll their own column.
- [ ] Clicking the radar scrolls the tactical view to the clicked spot, at the pane's corners as well as its middle.
- [ ] Sidebar tooltips appear over the control they belong to and are not clipped.

## Radar
- [ ] The minimap fills the pane and carries more detail than without the pack (it is resampled to the larger pane, not magnified).
- [ ] The white view outline tracks the tactical view and matches its proportions.
- [ ] Radar events (flashing cells) land on the right cells.
- [ ] In a multiplayer game, the player name and kill columns line up inside the pane.
- [ ] Change resolution in the options dialog during a game, from 1440p to 640×480 and back: the minimap is rebuilt at the new pane size rather than staying at the old zoom.

## Fonts
- [ ] Cameo captions and cost text are sharp 2× text, wrapped within the cameo.
- [ ] The credits readout and the mission timer are sharp and centred in the tab.
- [ ] Chat text over the tactical view is sharp, wraps at the same place as before, and the edit cursor sits at the end of the typed line.
- [ ] Unit tooltips over the map are sharp, sized to their text, and stay inside the view.
- [ ] Remove `8POINT.FNT` from the pack: the cameo text prints small rather than misplaced, and nothing is clipped.

## Cursor
- [ ] With a 2× `MOUSE.SHP` the pointer is the same size on screen as without the pack, and sharper.
- [ ] The pointer points at the right pixel: an attack cursor over a unit's edge selects that unit.
- [ ] `CursorScale=2` with a 2× pack gives the same size as `CursorScale=1` without it.
- [ ] The pointer is the same size in the menus as in play.

## Everything else
- [ ] The Windows dialogs and the main menu are unchanged by the pack.
- [ ] Save a game with the pack installed, remove the pack, and load the save: it loads and plays with the classic HUD.
- [ ] A multiplayer game between one client with the pack and one without stays in sync.
