# Play-test checklist — 01 Modern controls

Build: `fork/modern-controls`. Set `ModernControls=yes` under `[Options]` in `SUN.INI`, or tick
**Modern Controls** in Options → Game Controls. Test in a skirmish first, then a campaign mission.

## Selection (left button)
- [ ] Click own unit selects it; click another own unit switches to it.
- [ ] Drag a box selects units inside; Shift-drag adds.
- [ ] Shift-click own unit toggles it in/out of the selection.
- [ ] Click open ground, shroud, a tree, or a wall with units selected → selection cleared.
- [ ] Shift-click open ground → selection kept.
- [ ] Click an enemy unit with your units selected → selects the enemy (no attack).
- [ ] Double-click own unit still selects all of that type on screen (unchanged behaviour).

## Orders (right button)
- [ ] Right-click ground → move. Right-click enemy → attack. Right-click Tiberium with harvester → harvest.
- [ ] Right-click own APC/transport with infantry selected → enter. Right-click enemy building with engineer → capture.
- [ ] Ctrl + right-click on own unit → force attack. Alt + right-click → force move. Q held → queued move.
- [ ] Right-click with nothing selected → nothing happens (no cursor change beyond hover).
- [ ] Right-click own selectable unit while others are selected → no order, selection unchanged.

## Scrolling
- [ ] Hold right button and drag → coast scroll works; releasing after a drag gives **no** order.
- [ ] A quick right click (no movement) never scrolls.
- [ ] Edge scrolling and keyboard scrolling unchanged.

## Modes (classic buttons must apply in both schemes)
- [ ] Build placement: left places, right cancels.
- [ ] Repair / Sell / Power buttons: left applies, right cancels; selection not cleared by the cancel.
- [ ] Superweapon (ion cannon / cluster missile): left fires, right cancels.
- [ ] Waypoint mode: left places/picks waypoints, right exits.

## Radar
- [ ] Left-click minimap moves the view. Right-click minimap with units selected → move order to that spot.

## Persistence
- [ ] Toggle in Game Controls dialog → takes effect immediately; `SUN.INI` shows `ModernControls=yes`.
- [ ] Toggle off → classic behaviour fully restored (left orders, right deselects).

## Not affected (spot check)
- [ ] Sidebar clicks (left build, right cancel), hotkeys, chat, save/load.
