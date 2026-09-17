---
key: AssetScale
summary: How many pixels the tactical view draws for each pixel of the original artwork.
see_also: [AssetOverrides, UIScale, VoxelSupersample, ScreenWidth]
when_omitted:
  kind: value
  value: "1"
---

The tactical view projects the world onto a diamond tile of 48 by 48 pixels, half of it above the cell and half below, and every object on the map is placed and drawn against that tile. On a large display the engine already renders the frame at the display's own resolution, so the picture is sharp; what is small is the artwork, which was drawn for a tile of that size.

`AssetScale=2` doubles the tile to 96 by 96 and projects everything against the larger one. A cell covers four times the screen area, half as many cells fit across the view, and the radar view rectangle follows without further adjustment. Objects, their shadows, health bars, selection brackets, transport pips, effects and voxel vehicles are all placed and sized against the doubled tile.

Artwork drawn for the original tile is magnified to fit rather than left small, each pixel becoming a square of four. That is a plain magnification and adds no detail, but it keeps every sprite in the right place at the right size, so the view works with no artwork made for the larger tile and artwork can then be replaced a piece at a time. A shape recognised at the larger size is drawn as it is. Magnified frames are held so that a sprite redrawn every frame is magnified once; that store is bounded and the least recently drawn frames are dropped when it fills.

Terrain is drawn against the doubled tile too: the ground, the cliff and overhang artwork attached to a tile, the shroud and the fog. Original tile artwork is magnified the same way sprites are, so the mode needs no artwork made for it.

The setting changes only how the local picture is drawn. Distances, damage, sighting and movement are measured in leptons, which the scale does not touch, so a game saved, recorded or played over a network at one scale behaves the same at the other.

Values below one are raised to one and values above two are lowered to two. The setting is read when the game starts and is not read again, so a change takes effect the next time the game is launched.
