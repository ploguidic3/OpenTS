---
title: Project the tactical view at twice the pixel density
category: feature
release: 0.2.0
targets:
- type: key
  id: AssetScale
  effect: added
credit:
- Joe
---

`AssetScale=2` under `[Video]` in `sun.ini` projects the tactical view at 96 by 48 pixels per cell instead of 48 by 24. Objects, their shadows, effects and voxel vehicles are drawn to match, so a cell covers four times the screen area and the picture carries the detail of artwork drawn for it. Artwork drawn at the original size is magnified to fit, which keeps the view usable before any artwork for the larger size exists.

Terrain is drawn at the scale as well. The isometric tile rasteriser builds its span geometry for the scale in force rather than from the tables written for the original tile, and a tile's pixels, its depth data and the rectangular artwork attached to it for cliffs and overhangs are magnified with it. The shroud and the fog are magnified the same way.

One defect is known. An object standing against a building draws in front of it rather
than behind it, because the per-pixel depth that building artwork carries is measured
against the original tile and is still weighed wrongly against the depth of the rows it
sits on. `AssetScale=1` is unaffected.

The simulation is unchanged. Leptons, cells and the values derived from them keep their meanings, and the places where a screen measurement used to reach game state now take the original tile rather than the view's: shroud sighting, warhead damage falloff, the step a walking or crawling unit climbs, the world position an artwork offset names, and the position a light-casting shape records. Those give the same answer at either scale, so a save, a recording and a network game carry across.

The setting is read when the game starts and needs a restart to change, because the view geometry, the tile tables and the voxel bitmap are all built from it.
