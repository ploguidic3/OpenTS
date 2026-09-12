# Play-test checklist — 03a Integer HUD scale

Build: `fork/hud-scale`. Needs a retail install under `Run/`. Run the Debug build with `-WIN`
so the debug log is readable; the log line `UIScale <setting> resolves to <N> at <W>x<H>`
appears once at start and again after a resolution change. `UIScale=` goes under `[Video]`
in `SUN.INI`; delete the line or set `0` for the automatic value.

Expected sidebar widths: 336 px at N=2, 504 px at N=3. Expected tab strip heights: 32 px
at N=2, 48 px at N=3. Run the full list once per resolution; the setting column says what
to put in `SUN.INI` for that pass.

| Resolution | Setting | Expected N | Also try |
| --- | --- | --- | --- |
| 1920×1080 | omitted | 2 | `UIScale=1` (old layout), `UIScale=3` (still 2, the sidebar would be too short) |
| 2560×1440 | omitted | 2 | `UIScale=3` (480-line sidebar, 4 or 5 cameo rows) |
| 3840×2160 | omitted | 3 | `UIScale=2`, `UIScale=4` |

## Layout
- [ ] The sidebar is the expected width and reaches the bottom of the screen; the credits tab, radar frame, mode buttons, cameo strips and scroll arrows are all magnified the same amount with square pixels (no smoothing, no stretching in one axis only).
- [ ] The tab strip along the top is the expected height and the Options tab sits at the right end of the tactical view (left end with the sidebar on the right).
- [ ] The tactical map keeps its native size: a unit is the same size on screen as with `UIScale=1`.
- [ ] Where the screen height is not a multiple of N (2560×1440 with N=3), at most two black rows show under the sidebar and at most two black columns at the right end of the tab strip. Nothing else is uncovered.
- [ ] `UIScale=1` restores the old layout exactly. Take a screenshot of the same scene with `fork/asset-overrides` and compare; nothing may differ.

## Sidebar buttons
- [ ] Repair, sell, power and waypoint buttons press where they draw, including their very edges (click the top-left and bottom-right pixel of each).
- [ ] The pressed art appears in the button that was clicked, not offset by a few pixels.
- [ ] Each cameo starts production when clicked anywhere inside its picture, and right-click cancels it. Test the top and bottom cameo of both columns.
- [ ] The scroll arrows scroll their own column; the strip animation runs smoothly.
- [ ] Mouse wheel over the sidebar and over the map scrolls the sidebar as before.
- [ ] Clicking the sidebar backdrop between the controls does nothing on the map behind it.

## Tooltips
- [ ] Hovering a cameo shows its name and cost in a box magnified like the sidebar, inside the sidebar, and the box moves with the mouse column.
- [ ] Hovering the power bar shows the power readout; the region covers the whole bar.
- [ ] Hovering the mode buttons shows their names.
- [ ] Hovering a unit on the map shows its tooltip at N times the old size, drawn over the map with a green border, and it stays inside the tactical view near the right and bottom edges.
- [ ] With the sidebar on the right (`SidebarOnRight=yes` in `[Options]`), every tooltip above still lands on the correct surface and nothing is cut off at the seam.

## Radar
- [ ] Left-clicking the minimap moves the view so the clicked spot is centred, at every corner of the minimap.
- [ ] With a unit selected, right-clicking (modern controls) or left-clicking (classic) the minimap orders it to the clicked cell.
- [ ] The white view rectangle on the minimap follows the tactical view and matches its size.
- [ ] Radar movies (EVA in-radar video) play in the magnified pane.

## Tab strip, credits, power
- [ ] Clicking the Options tab opens the options menu; clicking the tab strip elsewhere does nothing.
- [ ] The credits readout ticks and stays legible; a mission with a timer shows the timer at the right end of the strip.
- [ ] The power bar pips fill the bar height and the tooltip region matches it.

## Chat and text
- [ ] In a skirmish, press Enter and type a message: the edit line and the text cursor are drawn N times larger over the map and the line wraps at the tactical view's width.
- [ ] Messages from other players (or your own in a LAN test) appear at the same size.

## Cursor
- [ ] With `CursorScale` omitted, the pointer is at least as large as the HUD multiple (2× at 1080p even though the frame is not enlarged).
- [ ] `CursorScale=1` keeps the small pointer whatever `UIScale` says.

## Resolution change at runtime
- [ ] Options → Display, change to a resolution with a different N (1280×720 ↔ 1920×1080) and back: the sidebar, tab strip, radar and tooltips all follow the new N, buttons hit where they draw, and the debug log shows the new resolve line. No stale black regions remain.
- [ ] Save, load (with the same resolution and with a different one): the sidebar rebuilds at the current N.
- [ ] Toggle the sidebar side in the options and confirm the buttons and radar clicks still land.

## Not affected
- [ ] Main menu, options dialogs, the loading screen and the score screen are unchanged.
- [ ] `ScaleMode`, `IntegerScaling` and windowed resizing behave as before; a window smaller than the frame still scales the whole picture down uniformly.
