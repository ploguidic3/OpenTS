# Prompt 01 — Modern controls option

Context to load: `@docs/modernization/kb/00-project-overview.md`,
`@docs/modernization/kb/01-input-and-controls.md`.

## Goal

Add an opt-in `ModernControls` setting: **left button selects and deselects, right
button issues orders**, in the style of Red Alert 3 / StarCraft II / the C&C Remastered
"modern" scheme. Off by default; the classic scheme is byte-for-byte unchanged when off.

## Verify first (plan mode)

Re-grep and confirm these still hold before designing:
- `ScrollClass::Message_Handler` in `code/scroll.cpp` dispatches `WM_LBUTTONDOWN/UP` and
  `WM_RBUTTONDOWN/UP` to `Map.Mouse_Left_Press/Release` and `Map.Mouse_Right_Press/Release`.
- `ScrollClass::Scroll_AI` routes a held right button to `Map.Scroll_Coast`.
- `DisplayClass::Mouse_Left_Release` contains both the selection logic and the
  `Active_Click` call; `DisplayClass::Mouse_Right_Release` ends in `Unselect_All()`.
- `ScrollClass::Mouse_Right_Release` returns early when `IsDragOperation`.
- `OptionsClass::Load_Settings/Save_Settings` read/write `[Options] AutoScroll`;
  `GameControlsClass` owns the Game Controls dialog with `IDC_EDGE_SCROLL`.
- `DisplayClass::TacticalClass::Action` does no click handling.
Show me the plan with the exact functions you will touch before editing.

## Behaviour specification (when `ModernControls=yes`)

Left button, tactical view:
- Press: arm the tentative rubber band exactly as today (`Mouse_Left_Press`).
- Drag past the threshold: rubber band as today.
- Release with a band: select the band contents (Shift adds), as today.
- Release without a band on a **selectable object of the player's**: select it
  (`Unselect_All` then `Select`, Shift/`ACTION_TOGGLE_SELECT` toggles, Ctrl-click and
  double-click type-select behave as today). Same for allied or enemy objects the classic
  scheme allows selecting when nothing is selected.
- Release without a band on **empty ground, shroud, terrain, or a non-selectable object**:
  `Unselect_All()`. This is the "left-click deselects" half.
- **Never** call `Active_Click` from the left button in modern mode, with these
  exceptions where the left button keeps its classic job because a mode is armed:
  building placement (`PendingObject`), `IsRepairMode`, `IsSellMode`, `IsPowerMode`,
  `IsTargettingMode != SUPER_NONE` (superweapon), and `IsWaypointMode`. In those modes
  left applies the mode and right cancels it, as classic does.

Right button, tactical view:
- Press: record `RightPressPoint`; do not deselect.
- Drag past the threshold: coast scroll exactly as today (`Scroll_Coast`,
  `IsDragOperation`). A drag never issues an order.
- Release without a drag: if a mode is armed, cancel it (the classic cascade minus the
  final `Unselect_All`). Otherwise, if there is a selection, compute
  `What_Action(cell, object, false)` for the cursor position and call `Active_Click`
  with it, exactly as the left button did classically — including `ACTION_MOVE`,
  `ACTION_ATTACK`, `ACTION_ENTER`, `ACTION_GUARD_AREA`, `ACTION_NOMOVE` (no-op), the
  Alt/Ctrl force modifiers via `Options.KeyForceMove*/KeyForceAttack*`, and the queue-move
  key. If there is no selection, do nothing.
- Right-click with a selection over one of your own selectable units: treat as the
  computed action (usually `ACTION_SELECT` → no-op or `ACTION_GUARD`/enter for
  transports). Do not select with the right button.

Cursor feedback (`Mouse_Left_Up`): keep showing the action cursor for the *right*
button's would-be action (move/attack/enter arrows) so the player sees what right-click
will do; over own units with nothing selected show the select cursor. Do the minimum
needed here; the switch is large.

Radar minimap: left-click recentres (already the no-action path); in modern mode a
right-click on the minimap with a selection issues the command instead of the left-click.
Sidebar: unchanged. Keyboard, hotkeys, `KEYBOARD.INI`: unchanged. `WM_RBUTTONUP` in
`winstub.cpp` (`Set_Scroll_Coasting_Allowed(false)`) unchanged. `SM_SWAPBUTTON` users:
document that the swap applies at `WWKeyboardClass::Down` only; do not fix here.

## Implementation guidance

- Put the branching in `ScrollClass::Message_Handler` and `Scroll_AI`, and split
  `DisplayClass::Mouse_Left_Release` into two protected helpers, e.g.
  `Select_Click(...)` and `Command_Click(...)`, so both schemes share code paths rather
  than duplicating the action cases. `Mouse_Right_Release` gains a parameter or a new
  sibling `Mouse_Right_Command(coord, cell, object, action)`.
- `ScrollClass::Message_Handler`'s `WM_RBUTTONUP` currently passes an unfilled `point`;
  resolve the real point from `lParam` for the modern path (and leave the classic call as
  it is).
- The option: `bool ModernControls` in `OptionsClass` beside `AutoScroll`, ctor default
  `false`, `[Options] ModernControls` in `Load_Settings`/`Save_Settings`.
- Dialog: new `IDC_MODERN_CONTROLS` in `code/language/language.h`, a `CONTROL` line in all
  three `IDD_OPT_CTRL_GAME_*` templates in `language.rc` (grow the SP template height as
  needed), primed in `Game_Controls_Dialog_Proc` `WM_INITDIALOG`, read in
  `GameControlsClass::Set`. Label: "Modern Controls (right-click orders)".
- Determinism: this changes only which local input produces which `EventClass`; no
  simulation, save, replay or network format changes. State that classification
  ("intentionally changed, opt-in, no compatibility boundary crossed") in the change
  record.
- Do not touch `radar.cpp` beyond the minimal right-release branch. Do not touch
  `gadget.cpp`.

## Deliverables

1. Code, built in Win32 Debug and Release.
2. `manual/changes/modern-controls-option.md` (category feature, `release: fork`,
   `targets: [{type: key, id: ModernControls, effect: added}]`) modelled on
   `manual/changes/edge-scrolling-option.md`.
3. `manual/content/keys/moderncontrols.md` modelled on `autoscroll.md`, `see_also:
   [ScrollMethod, AutoScroll]`, and an `ini-keys.yaml` entry with `_provenance` pointing at
   `OptionsClass::ModernControls`.
4. One paragraph in `manual/content/systems/target-selection.md` describing the modern
   scheme, and a sentence in `docs/UI_DESIGN.md` where it narrates the click route.
5. A play-test checklist for me (I run the game; you cannot): select/deselect, band
   select, Shift add, Ctrl force-attack, Alt force-move, Q queue, move/attack/enter/
   harvest/capture orders by right-click, right-drag scroll still works and never orders,
   building placement, repair/sell/power modes, superweapon targeting, waypoint mode,
   minimap, option round-trips through `SUN.INI` and the dialog, and that toggling the
   option off restores classic behaviour with no residual state.

## Report

Files changed, behaviour classification, exact build commands and results, tests run
(CTest) and not run (play-test), documentation updated.
