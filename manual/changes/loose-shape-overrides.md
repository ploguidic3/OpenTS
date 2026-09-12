---
title: Replace archived shape art with loose files
category: feature
release: 0.2.0
targets:
- type: key
  id: AssetOverrides
  effect: added
- type: format
  id: shp
  effect: changed
credit:
- Joe
---

A loose shape file in any folder the game searches now stands in for the archived copy of the same name: sidebar, radar, tab, cursor, cameo, object, animation and overlay art alike. A loose shape exactly twice the archived one's logical size is recorded as 2x art, and an `HDPACK.INI` beside the files can state the scale outright.

`AssetOverrides=no` under `[Video]` in `sun.ini` reads the archived shapes again without moving the files. The setting defaults to `yes`, and a game with no loose shape files draws exactly as before. The Debug build's `MOUSE.SHP` override, which was read from the game's own directory only, now follows the same search order and the same switch as every other shape.
