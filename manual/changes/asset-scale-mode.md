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

Terrain tiles are not drawn at the scale yet, so `AssetScale=2` is not yet playable: the ground keeps its original size while everything standing on it grows. The setting is complete apart from that and is left in place for the tile work to finish.

The simulation is unchanged. Leptons, cells and the values derived from them keep their meanings, and the places where a screen measurement used to reach game state now take the original tile rather than the view's: shroud sighting, warhead damage falloff, the step a walking or crawling unit climbs, and the world position an artwork offset names. Those give the same answer at either scale, so a save, a recording and a network game carry across.

The setting is read when the game starts and needs a restart to change, because the view geometry, the tile tables and the voxel bitmap are all built from it.
