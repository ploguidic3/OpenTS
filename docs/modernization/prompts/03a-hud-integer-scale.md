# Prompt 03a — Integer HUD scale

Context to load: `@docs/modernization/kb/00-project-overview.md`,
`@docs/modernization/kb/02-rendering-and-ui.md`.

## Goal

A `[Video] UIScale=` setting (0 = auto, 1..4) that draws the sidebar, tab strip, radar,
power bar, credits, tooltips, chat list and in-game bitmap text at an integer multiple
while the tactical view stays at native resolution. At 1440p the auto value is 2; at 4K
it is 3 (the auto rule: `max(1, round(ScreenHeight / 720))`, tell me if you disagree).
This is the nearest-neighbour step; 03b replaces the magnified art with real 2× art.

## Verify first (plan mode)

- `SidebarRect.Width = SIDE_WIDTH` unconditionally in `SidebarClass::Reposition_Sidebar`;
  `TacticalRect` origin `(SIDE_WIDTH, 16)` in `DisplayClass::Set_View_Dimensions`,
  `Change_Display_Mode` and `Resize_Tactical_View`.
- `Blit_Sidebar` copies `SidebarSurface` 1:1 into `VisibleSurface`; `DSurface::Blit_From`
  stretches with `StretchBlt` when rects differ and both are GDI-backed and not
  transparent.
- `TabClass` draws the tab strip directly into the visible/hidden surface at
  `TAB_HEIGHT*2`.
- `GadgetClass` hit tests use frame coordinates from `Get_Mouse_X/Y`.
- `CCToolTip` paints into game surfaces with fixed paddings.
List every hard-coded layout constant you find (start from the kb table) and show me
the plan: which are multiplied by `UIScale`, and where the single multiplication point
is.

## Design

1. `Options.UIScale` (`[Video] UIScale`, default 0). Resolved value `UI_Scale()` in a
   new `code/uiscale.h/.cpp` (fork), recomputed in `Video_Set_Mode`/`Change_Display_Mode`.
2. **Sidebar and tab strip render at 1× into their own surfaces and are magnified at
   composite time.** `SidebarSurface` stays 168 wide; a new `SidebarPresentRect` of
   `168*N` wide is what `Blit_Sidebar` targets with a stretch blit (the existing
   `StretchBlt` path; confirm it is hit — the source and dest must both be `DSurface`
   and the blit non-transparent). Same for the tab strip: render into a small
   `TabSurface` (new) and stretch it. `TacticalRect` origin becomes `(SIDE_WIDTH*N,
   16*N)` and its size shrinks accordingly; Z/A buffers follow `TacticalRect` already.
3. Input: gadgets that live in the HUD keep their 1× rects; translate the mouse point by
   `/N` (and subtract the HUD origin) when routing to HUD gadgets, or scale every gadget
   rect by `N` in `Reposition_Sidebar`. Choose one, apply it everywhere including tooltip
   regions, `RadarClass` clicks (`Map.Set_Tactical_Position` maths uses radar pixel
   offsets), sidebar scroll arrows, mode buttons and the mouse wheel.
4. Text drawn *outside* the HUD surfaces — chat/message list, unit name/health tooltips,
   waypoint labels, the credits ticker if it draws to `VisibleSurface` — must either move
   into a scaled overlay surface or use a scaled font. For this prompt: chat list and
   tooltips get a `FontClass` wrapper that prints at 1× into a scratch surface and
   stretch-blits by `N` (a new `Draw_Text_Scaled` helper). Note where quality suffers;
   03b fixes fonts properly.
5. Radar: `RadarClass` composes the minimap image into the sidebar surface at 1×; the
   stretch covers it. Radar click-to-position maths must use 1× coordinates.
6. Cursor: `CursorScale=0` already follows the frame scale; set the auto cursor scale to
   `max(frame scale, UIScale)` so the pointer matches the HUD. Keep `CursorScale>0`
   explicit values untouched.
7. `ScaleMode=PixelArt` already handles the frame→window step; the HUD stretch is a
   pure integer nearest blit, so no filtering artefacts.
8. Do **not** touch the Win32 OwnerDraw dialogs (menus, options). Their GDI fonts and
   640×400 backdrop are a separate project; note the state in the change record.

## Tests

None can render. Add a unit test for the `UI_Scale()` auto rule and for the point
translation helper (frame → HUD-local), synthetic inputs only.

## Deliverables

Code; Debug+Release; `manual/changes/ui-scale.md`; `manual/content/keys/uiscale.md`
(+ `ini-keys.yaml`); update `manual/content/keys/cursorscale.md` for the new auto rule;
a play-test checklist at 1920×1080 (N=2, sidebar 336 px), 2560×1440 (N=2) and
3840×2160 (N=3): all sidebar buttons click where they draw, cameo tooltips align, scroll
arrows work, radar clicks move the view to the right place, tab strip buttons, power
bar, credits, chat text legible, `UIScale=1` restores the old layout exactly, and
`Change_Display_Mode` at runtime does not leave stale rects.
