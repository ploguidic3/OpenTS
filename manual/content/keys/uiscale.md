---
key: UIScale
summary: How many times larger than its artwork the in-game HUD is drawn.
see_also: [CursorScale, AssetOverrides, ScreenWidth, ScreenHeight]
when_omitted:
  kind: value
  value: "0"
  note: Zero picks the multiple from the screen height.
---

The sidebar, the tab strip along the top, the radar, the power bar, the credits readout, the tooltips and the chat text are drawn at their artwork's size and then grown by this multiple when the picture is put together. Each pixel becomes a square, so the HUD looks blocky rather than smooth. The tactical map is not affected and keeps one map pixel per screen pixel.

Left at `0` the multiple follows the screen height: the height is divided by 720 and rounded to the nearest whole number, with one as the least. A 1280×720 or 1600×900 screen draws the HUD at its own size, 1920×1080 through 2560×1600 draw it twice as large, and 3840×2160 three times. `1` to `4` set the multiple directly, and a larger value counts as `4`.

A folder of replacement artwork that states its scale in [`HDPACK.INI`](/formats/hdpack-ini/) changes what the multiple means: the HUD is drawn from that artwork, and the multiple is rounded down to a whole multiple of the artwork's scale. With 2× artwork a screen of 3840×2160 therefore draws the HUD twice rather than three times as large, with each artwork pixel filling one screen pixel.

The multiple is lowered until the layout fits: the sidebar must keep at least 400 lines of its own and the tactical view at least 320 pixels of width. At 1920×1080 a value of `3` therefore behaves as `2`, and at 640×400 every value behaves as `1`.

The sidebar shows as many build cameo rows as fit its magnified height, so a larger multiple shows fewer rows on the same screen; scrolling covers the rest. Where the screen height is not a whole multiple of the setting, the few rows left under the sidebar stay black, and the tab strip may leave a strip of the same size at its right edge.

The setting is read when the game starts and again when the display resolution is changed in the options dialog. [`CursorScale`](/keys/cursorscale/) at `0` never draws the pointer smaller than this multiple, in the menus as well as in play. The menus and the Windows dialogs themselves keep their own size whatever the value.
