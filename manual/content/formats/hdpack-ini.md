---
format_id: hdpack-ini
title: HDPACK.INI
summary: States the scale a folder of replacement artwork is drawn at.
kind: file
source_files:
  - code/shapeload.cpp
  - code/fontload.cpp
  - code/uilayout.cpp
filenames:
  - HDPACK.INI
related:
  - type: key
    id: AssetOverrides
  - type: key
    id: UIScale
  - type: format
    id: shp
  - type: format
    id: opents-ini
---

A folder of replacement artwork puts this file beside the artwork it describes. It says how large that artwork is drawn compared with the game's own, which the game cannot always tell from the files themselves.

```ini title="HD\HDPACK.INI"
[UI]
Scale=2

[Scale]
SIDE1.SHP=2
```

The folder itself is made searchable by [`SearchPaths`](/formats/opents-ini/) and its files stand in for the archived ones while [`AssetOverrides`](/keys/assetoverrides/) is on. Without that the file is never read.

## `[UI] Scale`

`Scale` under `[UI]` states how many pixels of interface artwork the folder draws per pixel of the game's own: `2` for artwork drawn at twice the size. Omitted, or `1`, the interface is drawn from whatever artwork answers, at the classic size.

The first `HDPACK.INI` found in the searched folders answers for the whole interface; a second folder's file is not consulted for this key. The value is read once, before the surfaces the interface is drawn into are created, so a change takes effect the next time the game is launched.

The scale is honoured only where [`UIScale`](/keys/uiscale/) is a whole multiple of it. With `Scale=2`, a screen drawing the HUD twice as large draws the replacement artwork at its own size, one pixel per screen pixel; a screen drawing it four times as large grows each of the artwork's pixels to a square of two; and a HUD drawn at its classic size, whether by setting or because the screen is too small, ignores the replacement scale and draws every shape at the classic size. `UIScale` is rounded down to a multiple of this scale rather than landing between pixels, so an automatic value of three becomes two.

Artwork the folder leaves at the classic size is still drawn in its place, with each of its pixels grown to a square, so a folder that replaces part of the interface lays out as one picture. The debug log names the sidebar artwork this applies to. A shape whose enlarged frames outgrow what a shape file can describe is drawn at its own size instead, which for a backdrop leaves a visible gap; the log names it.

## `[Scale]`

Each entry names a file in the folder and the scale it is drawn at, as [SHP images](/formats/shp/#loose-files) describes. Unlike `[UI] Scale`, these entries speak only for the folder the file sits in.

## Fonts

A `.FNT` file in the folder is treated as replacement text at the pack's scale when its tallest glyph is exactly that many times the archived font's. A replacement font of the classic height prints at the classic size, which in an interface drawn larger leaves the text small. `12METFNT`, `KIA6PT`, `6POINT`, `8POINT`, `GRAD6FNT` and `EDITFNT` are the fonts the game reads this way.
