---
title: Draw the in-game HUD at a whole-number scale
category: feature
release: 0.2.0
targets:
- type: key
  id: UIScale
  effect: added
- type: key
  id: CursorScale
  effect: changed
credit:
- Joe
---

`UIScale=` under `[Video]` in `sun.ini` draws the sidebar, tab strip, radar, power bar, credits, tooltips and chat text at a whole multiple of their artwork size while the tactical view stays at the screen's own resolution. The HUD is still drawn at its original size and each of its pixels is grown to a square, so the art looks blocky rather than smooth. The sidebar keeps the number of build cameo rows that fit its magnified height, which is fewer than before on the same screen.

`0`, the default, picks the multiple from the screen height: one up to 900 lines, two from 1080 through 1600 lines, three at 2160 lines, so a screen of 1080 lines or more shows the larger HUD without any setting. `1` restores the previous layout exactly. The pointer's automatic size follows the larger of the frame enlargement and this multiple, in the menus too.

The menus, the options dialogs and the other Windows dialogs are otherwise unchanged and keep their own size. The setting is read when the game starts and again when the display resolution is changed in the options dialog. It changes only how the local picture is composed and touches no saved, recorded or networked state.
