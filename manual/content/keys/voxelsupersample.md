---
key: VoxelSupersample
summary: Whether voxel vehicles are drawn at twice their size and reduced back down.
see_also: [AssetOverrides, UIScale]
when_omitted:
  kind: value
  value: "no"
---

Vehicles, their turrets and barrels, the voxel parts of buildings, voxel projectiles and voxel debris are drawn from three dimensional models rather than from artwork, by a rasteriser that works at the size they appear on screen. With this on, the model is rasterised at twice that size and each square of four pixels is then reduced to the one palette colour that stands for it, so the finished object is the same size on screen as before but is decided from four samples a pixel rather than one.

The reduction works in palette colours, since the colour of a pixel is not resolved until the object is drawn into the frame. A square whose four samples are all drawn keeps the palette colour nearest their average, so two neighbouring shades of one shading ramp give the shade between them. A square with two samples or fewer drawn is left clear, so the object's edge is decided by which way the four samples fall rather than by the one sample under the finished pixel. That edge is still a hard one: the reduction has to name a single palette colour per pixel and does not know what the object is standing on, so an edge cannot be blended into the ground behind it.

House colours are kept apart from the rest of the palette. A square holding only house colours is averaged within the house range, and one that mixes house colours with fixed ones takes whichever it has more of rather than averaging across the boundary, so house colour is neither spread onto a hull nor lost from one.

Rasterising a pose costs about four times as much. A pose the game keeps is rasterised once, so most of that cost falls on the frames where a unit turns or a vehicle type first comes into view. A pose the game does not keep, such as a vehicle tilting as it crosses a slope or a gun barrel held at an angle of its own, is rasterised afresh every frame and pays the cost every frame.

The setting is read when the game starts and is not read again, so a change takes effect the next time the game is launched. It changes only how the local picture is drawn and touches no saved, recorded or networked state.
