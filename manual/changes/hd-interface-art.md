---
title: Draw the in-game HUD from replacement artwork at its own scale
category: feature
release: 0.2.0
targets:
- type: format
  id: hdpack-ini
  effect: added
- type: key
  id: UIScale
  effect: changed
- type: key
  id: AssetOverrides
  effect: changed
- type: key
  id: CursorScale
  effect: changed
credit:
- Joe
---

A folder of replacement artwork that states `Scale=2` under `[UI]` in `HDPACK.INI` now draws the sidebar, the tab strip, the radar, the power bar, the cameo strip and the pointer from that artwork at one screen pixel per artwork pixel, where before the classic artwork was magnified. The radar picture is resampled to the larger pane, so it carries the detail the pane can now show. A `.FNT` file in the folder whose glyphs are twice the archived font's height prints at that size, which covers the cameo captions, the credits, the mission timer, the chat list and the tooltips.

`UIScale` is rounded down to a whole multiple of the artwork's scale, since a fractional one would leave hit boxes and text between pixels. With 2× artwork the automatic value at 3840×2160 is therefore 2 rather than 3, and a HUD drawn at the classic size ignores the replacement scale.

Artwork the folder leaves at the classic size is enlarged as it is drawn, so a folder that replaces only some of the interface still lays out as one picture; the debug log names what it enlarged. `AssetOverrides=no` returns the game to the classic artwork and the magnified HUD. Nothing here changes saved, recorded or networked state.
