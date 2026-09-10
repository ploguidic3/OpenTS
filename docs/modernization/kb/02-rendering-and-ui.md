# Rendering and UI

Commit `0281b88`; re-grep. Read `00-project-overview.md` "The frame" first.

## 1. Presenter

- `VideoModeWidth/Height`, `WindowedMode` `code/video.cpp:35-42`. `Video_Present`
  `:263-282` hands `VisibleSurface->Get_Buffer()` + `Stride()` to `Backend_Present`.
  `Video_Present_If_Dirty` `:291-303` (paced to refresh, `:62-75`). Dirty marking from
  every `DSurface` write on the primary (`code/dsurface.cpp:287,410,486,516,574`). Called at
  the end of `Update_Visible_Surface` (`code/gscreen.cpp:577`).
- `VideoScaleInfo` `code/video.h:26-38`: `GameWidth/Height, DrawableWidth/Height,
  DestX/Y/Width/Height, ScaleX/Y`. `Update_Scale_Info` `code/video.cpp:83-112`: uniform
  fit, `IntegerScaling` floors when magnifying `:102-104`, centred (letterbox). Recomputed on
  init/set-mode/resize (`:169, :212, :231`), each followed by `Win_Cursor_Refresh`.
- Filter `code/video.cpp:118-130` maps `Options.ScaleMode` → `BACKEND_SCALE_*`.
- `code/vidscale.cpp` is coordinate mapping only: `Window_Point_To_Game :38`,
  `Game_Point_To_Window :54`, `Screen_*` `:69/:80`, `Clamp_To_Game :91`,
  `Get_Logical_Cursor_Pos :109` (legacy dialog controls sit at frame coordinates).
- `code/bgfxbackend.cpp`: views `VIEW_PRESCALE=0, VIEW_PRESENT=1` `:43-44`; init
  `:282-360` (single-threaded because presents happen inside dialog paint; renderer from
  `Options.Renderer`; embedded imgui shaders `:342-343`); frame texture `:402-441`
  (`B5G6R5` if sampleable else `BGRA8` + `_ConvertTable[65536]` widening `:66-67,
  :152-161, :486-495`); `Backend_Present` `:473-540` (upload, sampler flags, pixel-art
  prescale into `Ensure_Prescale_Target` `:249-270`, clear to black = letterbox bars
  `:530-533`, one quad `Submit_Quad :167`, `bgfx::frame()` `:539`).
- Surfaces: `Surface` `code/surface.h:44-128`; `XSurface::Blit_From` `code/xsurface.cpp:
  1269` → `Blit_Clip :1321` (does not clip coordinatedly when sizes differ, `:1386-1398`)
  → `Blit_Plain/Trans :1601-1637`. **Software blitters cannot scale** (`code/blit.cpp:412-
  415, :450-452`). `DSurface::Blit_From` `code/dsurface.cpp:473-521` is the one stretch
  path: sizes differ + both GDI-backed + not transparent → `StretchBlt` with
  `COLORONCOLOR` (nearest) `:509-511`. `DSurface::Create_Primary :223-241`.
- Resolution: early read `code/startup.cpp:577-588`; defaults `:611-619` (640×480);
  `VisibleRect/VideoModeWidth` set `:621-625`; full read `code/options.cpp:407-421`;
  mode list `EnumDisplayModes` `code/video.cpp:341-406`; display dialog bounds 640×400
  … 4096×4096 `code/mainopt.cpp:411-416`; runtime change `Change_Display_Mode`
  `code/mainopt.cpp:191-305` (reallocates surfaces, `Map.Set_View_Dimensions(temp)` with
  `temp.X = SIDE_WIDTH, temp.Y = 16` `:280-291`). Window creation `code/winstub.cpp:520-560`;
  `sun.manifest` declares PerMonitorV2 DPI awareness; `Win_Window_Drawable_Size :364-376`
  returns physical pixels.

## 2. HUD layout constants (the 640×480-era numbers)

`code/sidebar.h:59-77`: `SIDE_Y=148, SIDE_WIDTH=168, SIDE_BODY_Y=138, CREDITS_HEIGHT=16,
COLUMN_ONE_X=24, COLUMN_ONE_Y=26, COLUMN_TWO_X=92, COLUMN_TWO_Y=26, BUTTON_ONE_X=31,
BUTTON_ONE_Y=-9, BUTTON_SPACING=27, GADGET_CAMEO=1000, COLUMNS=2`.
`code/sidebar.h:183-201`: `OBJECT_HEIGHT=51, OBJECT_WIDTH=64, MAX_VISIBLE=4, MAX_SLOTS=60,
UP_X_OFFSET=5, UP_Y_OFFSET=25, DOWN_X_OFFSET=34, TEXT_X_OFFSET=30, TEXT_Y_OFFSET=2,
CAMEO_TEXT_Y_OFFSET=41, QUEUE_COUNT_X_OFFSET=60`.
Others: radar pane `code/radar.cpp:209-214` (`RadY=16, RadHeight=140, RadOffX=15,
RadOffY=12`), centring thresholds 140/108 `:937-944`; tab strip `code/tab.cpp:120-121`
(`EVA_WIDTH 80`, `TAB_HEIGHT 8` × `2 /*RESFACTOR*/` = 16 px, `:134, :249, :213`);
tactical origin `code/display.cpp:253-262` (`X = SIDE_WIDTH`, `Y = 16`), duplicated in
`code/mainopt.cpp:280-284` and `Resize_Tactical_View` `code/conquer.cpp:970-1002`; power
bar `code/power.h:66-69` (`POWER_X=8, POWER_Y=25, POWER_WIDTH=12, POWER_PIP_HEIGHT=4`),
pip count `code/power.cpp:268`; chat list font `7*2` `code/display.cpp:414-424`; startup
Z/A buffers hard-coded 480 `code/startup.cpp:658-662` (replaced by `TacticalRect` in
`code/display.cpp:371-390`); dialog backdrop "640x400 centered" `code/ownrdraw.cpp:6528-
6542`; GDI dialog font `MS Sans Serif` 14/12 px `code/ownrdraw.cpp:83-84`, `WS_Get_Font`
`code/windlg.cpp:657`.

Only two things follow screen height: `SidebarClass::Max_Visible` `code/sidebar.cpp:2848-
2856` (rows that fit between `SIDE1` and `SIDE3` art) and `SidebarRect`
`:2712-2715` (`Width = SIDE_WIDTH` unconditionally — **the sidebar is 168 px wide at 4K**).

## 3. Where controls are positioned

`SidebarClass::Reposition_Sidebar` `code/sidebar.cpp:2705-2818` is the single layout
routine (called from `DisplayClass::Set_View_Dimensions` `code/display.cpp:396` and
`sidebar.cpp:483, :1228`): background `:2729`; mode buttons chained by `BUTTON_SPACING`
`:2732-2745` with `DrawOffsetX = -SidebarRect.X`; scroll arrows `:2758-2768`; cameo slots
`:2771` + tooltip regions `(i | col<<8) + GADGET_CAMEO` `:2773-2779`; mode tooltips
`:2783-2806`. Radar button `RadarClass::Reposition_Sidebar` `code/radar.cpp:837-842`.
`Column[n].ObjectRect` `sidebar.cpp:323-326, :564-568`. `Set_View_Dimensions`
`code/display.cpp:361-428` is the other choke point (tactical rect, Z/A buffers).

Composition: `GScreenClass::Render` → `Blit_Display` `code/gscreen.cpp:443-448` →
`Update_Visible_Surface` `:479-579` (shake `:490-509`, sidebar-left offset `:511-514`,
**zoom crops the source rect `:522-539` so `Blit_From :575` stretches via `StretchBlt`**,
`Heal_Dialog_Controls :577`, present `:578`). `Blit_Sidebar` `code/sidebar.cpp:1010-1052`
copies `SidebarSurface` to `VisibleSurface`. Sidebar backdrop `Draw_It` `:929-1003`
(`SIDE1`, `SIDE2`×`Max_Visible()`, `SIDE3`, `ADDON`). Cameo strip `:1864-2060`.

## 4. HUD art (all via `MFCD::Retrieve`, mix-only)

`SidebarClass::One_Time` `code/sidebar.cpp:368-542`: `RCLOCK2, GCLOCK2, SIDEBAR.PAL, SELL,
POWER, WAYP, REPAIR, SIDE1/2/3, ADDON, R-UP, R-DN`; `DARKEN.SHP :1323`; per-house
`SIDEGDI1/2/3.SHP` `:2720-2722`. `RADAR.SHP` `code/radar.cpp:276`; `TABS.SHP`
`code/tab.cpp:352`; `POWERP.SHP` `code/power.cpp:187`; `MOUSE.SHP` `code/mouse.cpp:355-
361`; `PLACE.SHP`, `SHADOW.SHP` `code/display.cpp:250-251`; cameos `XXICON.SHP` or
`art.ini Cameo=` `code/techtype.cpp:687-695, :922-927`. PCX dialog art list
`code/ownrdraw.cpp:5168-5177, :6566-6592` (via `CCFileClass`, so loose files already win).

## 5. Gadgets, buttons, tooltips

`GadgetClass` positions are ints in frame coordinates; input polls `Get_Mouse_X/Y`
(already frame coordinates). `ShapeButtonClass::Set_Shape` `code/shapebtn.cpp:124-136`
**takes its size from the SHP** (`Get_Width/Height`) — a larger replacement SHP produces a
larger hit box automatically. Draw at `:182`. `TextButtonClass` sizes from
`FontClass::String_Pixel_Width`. Tooltips: `ToolTip` `code/tooltip.h:20-46` (region in
frame coordinates); `CCToolTip::Update` `code/cctooltip.cpp:35-91` (+4/+3 padding, +16
nudge, fixed), `Draw :139-177`. `UI.INI [Ingame]` (`code/uicontrol.cpp`) has no layout
or scale keys.

## 6. Fonts

`FontClass` `code/font.h:44-59` — no scale parameter. `WWFontClass` (`code/wwfont.h:44`,
header struct `:103-113`) reads legacy `.FNT`; `Print` `code/wwfont.cpp:440+` writes one
source pixel per destination pixel (`:522`). Loaded fonts `code/init.cpp:2190-2229`:
`12METFNT` (`Metal12FontPtr`), `KIA6PT` (`MapFontPtr`), `6POINT` (`Font6Ptr`), `EDITFNT`,
`8POINT` (`Font8Ptr`), `GRAD6FNT` (`GradFont6Ptr`); globals `code/_font.cpp:23-29`.
`MSFont` `code/msfont.h:19-91` — graphic-menu font whose glyphs are SHP frames (three per
character). Win32 dialogs use GDI TrueType, so they already rasterise at any size.

## 7. Cursor

Hardware Win32 cursor, already scaled: `code/wincursor.cpp` — `Cursor_Scale :54-71`
(`CursorScale` 0 = round(min(ScaleX, ScaleY)), clamped 1..8), `Build_Cursor :81-172`
(nearest box replication `:137-143`, hotspot × scale), 384-entry cache. `WWMouseClass`
(`code/wwmouse.cpp`) is a shim. Art `MOUSE.SHP`; hotspots `code/mouse.cpp:63-65, :213-231`.
Manual `manual/content/keys/cursorscale.md`.

## 8. `docs/UI_DESIGN.md` (proposal, unimplemented, 781 lines)

Plan: replace OwnerDraw Win32 dialogs with RmlUi documents rendered as a bgfx overlay at
physical resolution (`:196-215`: `Backend_Present` stops calling `bgfx::frame()`, new
`Backend_End_Frame`, `UI_Render_Overlay` between). Coordinates `:258-288`: density-
independent ratio `min(ScaleX, ScaleY)`, one `dp` = one game logical unit; pointer input
is client px minus dest origin. Fonts via FreeType `:522-530`. Assets through `CCFileClass`
with `name.shp#frame` decoding `:494-520`. Sidebar migration is step 14, last, after
OwnerDraw is retired (`:578-622`, `:725-726`). Explicit non-goal `:132`: no user UI
scale setting. `docs/DIRECTION.md` and `RATIONALE.md` say nothing about rendering.

## 9. Existing scaling

Tactical zoom: `Tactical::ZoomFactor` (`code/tactical.h:295`, init `tactical.cpp:107`,
serialised `:3757`) applied only in `Update_Visible_Surface` as a crop-and-stretch; driven
by `TAction_ZOOM_IN/OUT` `code/taction.cpp:2045-2070` with `Rule->ZoomInFactor`
(`code/rules.cpp:406, :1075`). No draw-time scale anywhere. `Sidebar::Zoom_Mode_Control`
is the radar pane mode cycle, unrelated. No TODOs about UI scale exist in `code/`.

## 10. Levers for a scaled HUD, cheapest first

1. Present-time (exists): low `ScreenWidth/Height` + PixelArt magnification. Scales the
   tactical view too — not what we want at 4K.
2. **Composite-time HUD magnification** (prompt 03a): draw the HUD into `SidebarSurface`
   and the tab strip at 1× as now, blit to `VisibleSurface` at integer `N` through
   `DSurface::Blit_From`'s `StretchBlt` path; make every layout constant, hit rect and
   tooltip region scale-aware at the two choke points (`Reposition_Sidebar`,
   `Set_View_Dimensions`) plus `TacticalRect` origin (`SIDE_WIDTH*N`, `16*N`). Radar image
   composition (`code/radar.cpp:911-953`) and `Blit_Sidebar` need the same treatment.
   Mouse input arrives in frame coordinates already; divide by `N` for HUD gadget hit
   tests or scale the gadget rects — pick one and apply everywhere.
3. **HD HUD art** (prompt 03b): 2× SHPs for every asset in §4, loaded through the override
   path from `05-file-resolution.md`; 2× `.FNT` fonts (needs a writer — format is
   documented by `WWFontClass` header struct); `ShapeButtonClass` picks up sizes itself;
   text offsets in §2 become `×N`.
4. The documented direction: RmlUi overlay. Large; only worth it if you also want new
   menus. Not in this kit.
