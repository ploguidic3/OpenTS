---
title: Draw voxel vehicles from four times as many samples
category: feature
release: 0.2.0
targets:
- type: key
  id: VoxelSupersample
  effect: added
credit:
- Joe
---

`VoxelSupersample=yes` under `[Video]` in `sun.ini` rasterises vehicles, turrets, barrels, the voxel parts of buildings, voxel projectiles and voxel debris at twice their size and reduces each square of four pixels back to one palette colour. The object is the same size on screen as before and is decided from four samples a pixel rather than one.

The reduction works in palette colours, because a pixel's colour is not resolved until the object is drawn into the frame. A fully drawn square keeps the palette colour nearest the average of its four, so two shades of one shading ramp give the shade between them; a square with two samples or fewer drawn is left clear, so an edge follows the four samples rather than the one under the finished pixel. The edge is still hard: the reduction names one palette colour per pixel and cannot know what the object is standing on.

A square of house colours is averaged within the house range, and a square mixing house colours with fixed ones takes whichever it has more of, so house colour is neither spread onto a hull nor lost from one.

Rasterising a pose costs about four times as much. A pose the game keeps is rasterised once; one it does not keep, such as a vehicle tilting on a slope, is rasterised every frame as before. The setting is read when the game starts and needs a restart to change. It touches no saved, recorded or networked state.

The rasteriser now takes a whole-number scale rather than a fixed bitmap size, which is what a later artwork scale will draw vehicles through.
