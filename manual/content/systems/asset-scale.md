---
title: HD asset scale
summary: "Projects the tactical view at a whole multiple of the original tile, drawing artwork made for the larger tile as it is and magnifying the rest to fit."
category: rendering-presentation
keys:
  - AssetScale
  - AssetOverrides
  - VoxelSupersample
---

The tactical view is built around a diamond tile of 48 by 48 pixels. Every projection the view makes, and every offset the artwork carries, is measured against that tile. `AssetScale` multiplies it by a whole number, so at `AssetScale=2` the tile is 96 by 96, a cell covers four times the screen area, and half as many cells fit across the view.

Only the picture changes. The simulation measures in leptons, of which there are 256 to a cell whatever the tile is, so the same orders produce the same result at either scale.

## What a shape is drawn at

Each shape file is recognised at a scale of its own when it is fetched. A shape that came from the archives, or from a loose file with nothing to mark it otherwise, counts as original artwork at scale 1. A shape recognised at the view's scale is drawn as it is, pixel for pixel.

A shape below the view's scale is magnified by the whole number between them, each pixel becoming a square of that size, and drawn from the magnified copy. The magnification is nearest neighbour: every output pixel is one of the input pixels, so a palette index is never blended into another one. That matters because the artwork is palette indices rather than colours, and a blended index would be a different colour rather than an intermediate one. It matters most for the transparent index, which stays transparent, and for the house-colour range, which stays house colour.

Magnifying adds no detail. A magnified sprite is the original sprite, four times the area. What it buys is that the view is complete and correctly laid out before any artwork for the larger tile exists, so a set of artwork can be built and installed a piece at a time, with every piece not yet replaced still drawn in the right place at the right size.

Magnified frames are held so that a sprite redrawn every frame is magnified once rather than every time. The store is bounded, and the frames drawn longest ago are dropped when it fills.

## Tagging artwork at a larger scale

Larger artwork is supplied as a loose shape file in one of the searched folders, which `AssetOverrides` must be on for the game to look for. The scale a loose shape is recognised at is recorded when it is fetched, and a shape recorded at the view's scale is drawn without magnification.

Artwork for the larger tile has to stay palette artwork on the original palettes. Shapes are drawn as palette indices that are translated to screen colours inside the blitter, one index at a time, so a shape in full colour cannot be drawn at all. House colour is carried by a particular range of indices, which artwork made by enlarging an original has to preserve rather than blend across.

## Voxels

Vehicles and the other voxel objects are rasterised rather than drawn from shapes, so they have no artwork to replace. Their rasteriser takes a scale of its own and is set to the view's, which draws them at the larger size directly with no magnification step. `VoxelSupersample` is separate: it rasterises at twice whatever the view asks for and reduces back down, trading time for a better-sampled edge, and it still does that at either scale.

## What is not scaled

Terrain tiles are not drawn at the scale yet. Until they are, a scale above one leaves the ground at its original size while everything standing on it is drawn larger.

The radar is composed from the colour of each cell rather than from view pixels, so it is unaffected. The heads-up display has its own setting, `UIScale`, and is unaffected as well.
