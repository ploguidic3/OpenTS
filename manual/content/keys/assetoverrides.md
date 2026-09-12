---
key: AssetOverrides
summary: Whether a loose shape file in the searched folders stands in for the archived one.
see_also: [CursorScale]
when_omitted:
  kind: value
  value: "yes"
---

With the setting on, every shape the game fetches by name is first looked for as a loose file, in [the order files are searched for in](/formats/opents-ini/#the-order-files-are-searched-for-in), and a file found there is drawn instead of the copy in the [MIX archives](/formats/mix/). [SHP images](/formats/shp/#loose-files) covers which names are fetched this way and how a loose file's scale is recognised. The `.FNT` text fonts, sound samples and the palettes the game keeps resident are read from the archives whatever the setting says.

`AssetOverrides=no` under `[Video]` in `sun.ini` turns the loose files off without moving them: a shape the archives hold is read from the archives, as it is by a game that has no loose files. The menu fonts and animations that already fell back to a loose file when the archives lacked the name still do.

The setting is read before the first shape is fetched and is not read again, so a change to it takes effect the next time the game is launched. Each shape name is looked for on disk once, the first time it is fetched, and the answer is kept for the rest of the run. The debug log names each loose file as it is read, with its size and the scale it was given.
