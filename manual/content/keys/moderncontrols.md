---
key: ModernControls
summary: Gives orders with the right mouse button and keeps the left button for selecting.
see_also: [ScrollMethod, AutoScroll]
when_omitted:
  kind: value
  value: "no"
---

`ModernControls=yes` swaps the tactical map over to the button layout of later real-time strategy games. A left click selects the object under the pointer, a left drag draws the selection box, and a left click on open ground, shroud or anything that cannot be selected clears the selection. Shift keeps its meaning: a Shift-click adds to or removes from the selection, and a Shift-click on open ground leaves the selection alone.

A right click gives the selection its order. The pointer shows the same move, attack, enter, capture and harvest cursors it always has, and the order it shows is the one the right button gives. The Alt force-move, Ctrl force-attack and Q queued-move modifiers act on the right button. A right click with nothing selected does nothing.

Holding the right button and dragging still coast scrolls the map exactly as [`ScrollMethod`](/keys/scrollmethod/) describes; a drag never gives an order, and a click that has not travelled the drag distance never scrolls.

Placing a structure, and the repair, sell, power, superweapon and waypoint modes, keep the classic buttons under either setting: the left button applies the mode and the right button cancels it. On the radar a left click moves the view and a right click gives the order.

The in-game game controls dialog carries the same switch as a Modern Controls check box and writes the choice back to `sun.ini`. Changing it takes effect at once. The sidebar, hotkeys and `keyboard.ini` are unaffected, and the setting changes only which button produces which order, so it has no bearing on saved games, recordings or network play.
