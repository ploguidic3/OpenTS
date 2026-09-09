# Input and controls

How a mouse button becomes an order, with every handler located. Commit `0281b88`; re-grep.

## Classic behaviour being replaced

Left button: press arms a rubber band; release with no band selects the object under the
cursor, or issues the action the cursor shows (move/attack/enter/…) if units are selected.
Right button: release cancels the current mode or deselects everything; right-drag beyond
the system drag threshold coast-scrolls the map. Sidebar and radar have their own gadgets.

## 1. Button events entering the engine

- Window procedure `Windows_Procedure`, `code/winstub.cpp`:
  - `:184` `Route_Mouse_Message(...)` (`code/msgroute.cpp:31-48`) re-targets under video
    scaling; `:192` **`Map.Message_Handler(hwnd, message, wParam, lParam)`** — the tactical
    path; `:312-314` `WM_RBUTTONUP → Game_Window_On_Right_Mouse_Up()` →
    `Map.Set_Scroll_Coasting_Allowed(false)` (`code/gamewindow.cpp:51`); `:319-321`
    `WM_MOUSEWHEEL` → `Execute_Command("SidebarUp"/"SidebarDown")`; `:346`
    `Keyboard->Message_Handler(...)` — the keyboard-queue path.
- `WWKeyboardClass::Message_Handler` `code/keyboard.cpp:609`: `WM_LBUTTONDOWN/UP` →
  `Put_Mouse_Message(VK_LBUTTON, x, y[, true])` (`:661-671`), right at `:717-727`, double
  clicks at `:678-683`/`:734-739`. `Put_Mouse_Message` `:276` pushes key,x,y and ORs
  `WWKEY_RLS_BIT` (`code/keyboard.h:41`) on release. `Buff_Get` `:113` pops and sets
  `MousePos`. `Down(key)` `:381` reads `GetAsyncKeyState` and honours `SM_SWAPBUTTON`
  (`:385-387`) — **only here, not in `Message_Handler`**.
- Key codes `code/keyboard.h`: `KN_LMOUSE = VK_LBUTTON` (`:599`), `KN_RMOUSE` (`:619`),
  `KN_RLSE_BIT`, `KN_BUTTON` aliases `:639-644`.
- `GadgetClass::Input` `code/gadget.cpp:525-584` turns queue entries into flags
  `LEFTPRESS/LEFTHELD/LEFTRELEASE/LEFTUP/RIGHT*` (`code/gadget.h:107-114`). Sidebar and
  radar gadgets consume these; **the tactical view does not** (see §2g).
- `docs/UI_DESIGN.md` ~`:45-58` narrates this chain; update it if routing changes.

## 2. Tactical click dispatch

### 2a. The real dispatcher: `ScrollClass::Message_Handler` (`code/scroll.cpp:607`)

Declared `code/scroll.h:102`. Early-outs `:609-643` (`!TacticalActive`, no scenario,
`!GameActive`, no `TacticalMap`, no `MouseCursor`, `SpecialDialog != SDLG_NONE`,
`IgnoreInput`). Cases `:645-705`:

| Message | Does |
| --- | --- |
| `WM_LBUTTONDOWN` `:646` | if `!IsMouseDown`: `point = pts - TacticalRect.XY`; `Map.Mouse_Left_Up(cell, shadow, object, What_Action(cell, object, true))`; `Map.Mouse_Left_Press(point)`; `SetCapture`; `IsMouseDown = true` |
| `WM_LBUTTONUP` `:662` | `Map.Mouse_Left_Release(coord, cell, object, What_Action(cell, object, false))` `:670`; `IsMouseDown = false`; `ReleaseCapture` |
| `WM_RBUTTONDOWN` `:676` | `Map.Mouse_Right_Press(point)` `:684`; `SetCapture`; `IsMouseDown = true` |
| `WM_RBUTTONUP` `:691` | `Map.Mouse_Right_Release(point)` `:693` (note: `point` is default-constructed, never filled from `lParam`); `Abort_Drag_Select()` `:694`; release capture |
| `WM_CAPTURECHANGED` `:699` | `Abort_Drag_Select()` |

**This is the primary branch point for a modern-controls flag.**

### 2b. Per-poll tracking: `ScrollClass::Scroll_AI` (`code/scroll.cpp:539`)

`:553-561` if `IsMouseDown`: `Keyboard->Down(KN_LMOUSE)` → `Map.Mouse_Left_Held(point)`,
else if `Keyboard->Down(KN_RMOUSE)` → **`Map.Scroll_Coast(point)`**; return. Otherwise
`:562-576`: `Resolve_Point`, `HoverObject = object`, `Map.Mouse_Left_Up(...)` (cursor
feedback), then `if (Options.AutoScroll && !Debug_Map) Scroll_Edge(point)`.
Called from `ScrollClass::AI` `:156` ← `MouseClass::AI` `code/mouse.cpp:312` ←
`DisplayClass::AI` `code/display.cpp:886` ← `GScreenClass::Input` `code/gscreen.cpp:269`
← `Map.Input` at `code/mainloop.cpp:303,596,615`, `code/conquer.cpp:890`,
`code/queue.cpp:1116,1480`; followed by `Keyboard_Process(input)` `code/mainloop.cpp:477`.

### 2c. `DisplayClass` handlers (`code/display.h:200-206`, protected virtual)

```cpp
virtual void Mouse_Right_Press(Point2D const & point = Point2D());
virtual void Mouse_Left_Press(Point2D const & point);
virtual void Mouse_Left_Up(Cell const & cell, bool shadow, ObjectClass * object, ActionType action, bool wsmall = false);
virtual void Mouse_Left_Held(Point2D const & point);
virtual void Mouse_Left_Release(Coord const & coord, Cell const & cell, ObjectClass * object, ActionType action, bool wsmall = false);
virtual void Mouse_Right_Release(Point2D const & point = Point2D());
```

- `Mouse_Right_Press` `code/display.cpp:1830` — empty.
- `Mouse_Right_Release` `:1842-1874` — cancel cascade: pending placement → `IsRepairMode`
  → `IsSellMode` → `IsPowerMode` → `IsTargettingMode` → `IsWaypointMode` →
  **`Unselect_All()` `:1866`** → `Set_Default_Mouse(MOUSE_NORMAL, Map.IsSmall)`.
- `Mouse_Left_Up` `:1901` — cursor feedback: `IsTentative = false` `:1903`, waypoint colour
  `:1922`, then a large `ActionType → Set_Default_Mouse(MOUSE_*)` switch `~:1970-2260`.
- `Mouse_Left_Release` `:2288` — the workhorse: `:2294` placement (`EventClass::PLACE`);
  `:2318-2332` band commit (`if (!Keyboard->Down(KN_LSHIFT)) Unselect_All();
  TacticalMap->Select_Rubber_Band(Bandbox_Selection_Callback); IsRubberBand=false;
  IsTentative=false; drag_select_aborted=true`); `:2337-2346` `ACTION_TOGGLE_SELECT`;
  `:2351-2360` `ACTION_SELECT`/`ACTION_NONE` on a selectable → `Unselect_All();
  object->Select()`; `:2366+` `if (action != ACTION_NONE && action != ACTION_SELECT)
  Active_Click(object, cell, action)` plus `ACTION_TOGGLE_POWER`, waypoint, `REPAIR`,
  `SELL`, `SELL_UNIT`, super-weapon `SPECIAL_PLACE` cases.
  **Split point: selection half stays on LMB, `Active_Click` half moves to RMB.**
- `Mouse_Left_Press` `:2521` — arms the band unless a mode is active:
  `if (!IsRepairMode && !IsPowerMode && !IsSellMode && IsTargettingMode == SUPER_NONE &&
  !PendingObject) { IsTentative = true; BandX=NewX=point.X; BandY=NewY=point.Y; }`
- `Mouse_Left_Held` `:2564` — band grows; starts when `(point - Band).Length() > 4` `:2588`.
- `Bandbox_Selection_Callback` `:2477` — player-owned selectables; buildings only if they
  undeploy.
- `Abort_Drag_Select` `:4210`; `Active_Click(object, cell, action)` `:3949` — iterates
  the selection, re-derives per-object `What_Action`, calls `Active_Click_With`
  (`:3976, :3986, :3993, :4031, :4116, :4124, :4142`).

### 2d. `ScrollClass` overrides (`code/scroll.h:108-109, :99`)

- `Mouse_Right_Press` `code/scroll.cpp:884` — `BASECLASS::Mouse_Right_Press; RightPressPoint
  = point; IsDragOperation = false; Show_Mouse()`.
- `Mouse_Right_Release` `:867` — `if (IsDragOperation) { Set_Default_Mouse(MOUSE_NORMAL);
  IsDragOperation = false; return; }` then base. **A right-drag scroll does not deselect.**
- `Abort_Drag_Select` `:901` — base + `IsMouseDown = false` + `ReleaseCapture`.

### 2e. Hit test and action resolution

- `ScrollClass::Resolve_Point(point, cell&, coord&, object*&, fog&, shadow&)`
  `code/scroll.cpp:175` — `TacticalMap->Pixel_To_Cell/Coord`, `Get_Selectable_Object`,
  hides cloaked enemies.
- `ScrollClass::What_Action(cell, object, check_fog)` `:255` — with a selection delegates
  to `Best_Selected_Object()->What_Action(...)`; else `ACTION_SELECT` `:290`; mode
  overrides `:293-375` (repair, power, waypoint, sell, targeting).
- Per-object: `TechnoClass::What_Action(ObjectClass const*)` ~`code/techno.cpp:4247-4330`
  (`Options.KeyForceMove*/KeyForceAttack*/KeySelect*` at `:4297-4299`; `ACTION_MOVE :4311`,
  `ACTION_TOGGLE_SELECT :4322`); `TechnoClass::What_Action(Cell const&, bool check_fog,
  bool disallow_force)` `:4387`; `FootClass` `code/foot.cpp:4513, :4544`; `BuildingClass`
  `code/building.cpp:3836, :3933`; `AircraftClass` `code/aircraft.cpp:2020, :2096`;
  `InfantryClass` `code/infantry.cpp:2698`.
- `Active_Click_With(action, object|cell, is_waypoint)`: `code/foot.cpp:1612, :1756`,
  `code/building.cpp:2609, :2659`, `code/aircraft.cpp:1915, :1962`.
- `ActionType` enum `code/action.hh:27-81`.

### 2f. Rubber band — `Tactical` (`code/tactical.cpp`)

`Start_Rubber_Band :2999`, `Modify_Rubber_Band :3013`, `Select_Rubber_Band(cb) :3027`,
`End_Rubber_Band :3056`, `Draw_Rubber_Band :3068` (called `:1308`), `Select_These(rect,
cb) :3264`. State `RubberBandStart/End` `code/tactical.h:383-384`, serialised
`tactical.cpp:3771-3772`; `Point2D(0,0)` is the no-band sentinel. `Unselect_All()`
`code/conquer.cpp:1128`.

### 2g. Quirk

`DisplayClass::TacticalClass::Action` (`code/display.cpp:1772`, gadget declared
`display.h:284-297`) no longer handles clicks — it only sets the cursor cell. Do not add
logic there.

## 3. What must not change

- **Right-drag scroll:** `ScrollClass::Scroll_Coast` `code/scroll.cpp:718` (body through ~`:880`); threshold
  `GetSystemMetrics(SM_CXDRAG)*2` `:791-796` → `IsDragOperation = true`; if `IsRubberBand`
  it calls `Mouse_Right_Release(Point2D(-1,-1))` instead `:800-808`; speed from
  `Options.Get_Scroll_Method()`/`ScrollRate` `:822-880`. State `code/scroll.h:63-84`
  (`IsCoastScrollAllowed, RightPressPoint, IsDragOperation, IsMouseDown`). Holding RMB also
  speeds edge scroll `:493-495`. `Is_Scrolling` `:583`.
- **Radar:** `RadarClass::RTacticalClass::Action` `code/radar.cpp:500` — `LEFTUP` →
  `Mouse_Left_Up(..., true)` `:596`; `LEFTRELEASE && !drag_select_aborted` →
  `Mouse_Left_Release(..., true)` `:603-605`; `RIGHTRELEASE` → `Mouse_Right_Release`
  `:607-609`; `LEFTPRESS` with no action recentres `:611+`. Actions limited to
  MOVE/NOMOVE/ATTACK/ENTER/CAPTURE/SABOTAGE/HARVEST `:570-583`. `RTacticalClass` is a
  friend of `ScrollClass` (`scroll.h:46`).
- **Sidebar:** `SidebarClass::StripClass::SelectClass::Action` `code/sidebar.cpp:2304` —
  right already cancels/suspends production (`:2340, :2374`); `SBGadgetClass::Action`
  `:2538`. These go through `GadgetClass::Input`, not `Message_Handler`.
- `drag_select_aborted` global `code/globals.cpp:466`, cleared each frame
  `code/mainloop.cpp:318`.
- `Keyboard_Process` `code/mainloop.cpp:477` — hotkeys, `KN_ESC/KN_SPACE → Queue_Options`,
  `KN_TAB → Zoom_Mode_Control`.

## 4. Options and dialog plumbing

- `OptionsClass` `code/options.h:39`; flags are public members (`ScrollMethod :90`,
  `ScrollRate :91`, `AutoScroll :92`, `ActionLines :123`, `ToolTips :128`; modifier keys
  `:217-224`, defaults `code/options.cpp:142-148`: `KeyForceMove=KN_LALT`,
  `KeyForceAttack=KN_LCTRL`, `KeySelect=KN_LSHIFT`, `KeyQueueMove=KN_Q`).
- Persistence: `Load_Settings` `code/options.cpp:357` (`[Options]` keys `:364-405`),
  `Save_Settings` `:457` (`:466-495`). A `ModernControls` bool: `Get_Bool("Options",
  "ModernControls", ModernControls)` near `:399`, `Put_Bool` near `:475`, ctor init
  `:111-153`, member beside `AutoScroll` `options.h:92`.
- In-game Game Controls dialog `code/gamedlg.cpp`: `GameControlsClass::Dialog` `:102`
  (template chosen `:109-116`, OK → `Set(); Options.Save_Settings()` `:132-135`); `Set`
  `:152` (`IDC_SCROLL_COASTING :205`, `IDC_EDGE_SCROLL :210`); `Game_Controls_Dialog_Proc`
  `:233` (`WM_INITDIALOG` primes check boxes `:263-286` via `Button_SetCheck`). Reached
  via `code/goptions.cpp:248-252` → `SDLG_SETTINGS` → `code/conquer.cpp:252-257`.
- Dialog resources `code/language/language.h` (`IDD_OPT_CTRL_GAME_SP 245 :262`,
  `IDC_SCROLL_COASTING 1306 :915`, `IDC_EDGE_SCROLL 1714 :1167`) and
  `code/language/language.rc` templates at `:224` (MP), `:263` (SP), ~`:1599` (WOL), e.g.
  `:298-301`:
  ```
  CONTROL "Scroll Coasting",IDC_SCROLL_COASTING,"Button",BS_AUTOCHECKBOX | BS_FLAT,146,119,128,10
  CONTROL "Edge Scrolling",IDC_EDGE_SCROLL,"Button",BS_AUTOCHECKBOX | BS_FLAT,22,135,124,10
  ```
  A new checkbox needs an `IDC_` and a `CONTROL` line in all three templates; the SP
  template is 179 dlu tall and full.
- Out-of-game: `Main_Options_Dialog` `code/mainopt.cpp:58` → `IDC_OPTMAIN_GAME_SETTINGS
  :136` → `GameControlsClass().Dialog()`; checkbox idiom `:444-447`/`:485-488`.
- Command/hotkey system: `CommandClass` `code/command.h:12`; `HotkeyCommands`
  `code/_command.cpp:19`; `Init_Commands` ~`code/init.cpp:5650-5872`; `Init_Hotkeys`
  `:5878` (`KEYBOARD.INI [Hotkey]`); `Execute_Command(name)` `:5921`; `Hotkey_Dialog`
  `code/options.cpp:817`.
- Precedents for a flag: `AutoScroll` (`[Options]` + Game Controls checkbox — **use this**),
  `UI.INI [Ingame]` (`code/uicontrol.cpp:28-54`), `OPENTS.INI`. No `[Fixes]`/`[Extensions]`
  section exists.

## 5. Manual and tests

- Templates: `manual/changes/edge-scrolling-option.md`, `manual/content/keys/autoscroll.md`,
  `manual/data/ini-keys.yaml` entry for `ScrollMethod` (~`:19095-19111`). Neighbours to
  `see_also`: `scrollmethod.md`, `scrollrate.md`. Prose homes:
  `manual/content/systems/target-selection.md`, `systems/sidebar.md`,
  `using/configuration-files.md`. `manual/data/command-adapters.yaml:340-344` records
  `GadgetClass::Input`'s mouse-code uses as deliberately-not-a-command.
- No test touches input. `tests/ini` and `tests/deploymentconfig` are the round-trip
  patterns if the option gets a unit test.
