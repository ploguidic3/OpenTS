---
format_id: shp
title: SHP images
summary: Stores indexed two-dimensional image frames used by sprites and interface graphics.
kind: binary
extensions:
  - .SHP
role: image
source_files:
  - code/shapeset.h
  - code/shapeload.cpp
  - code/objtype.cpp
  - code/builtype.cpp
related:
  - type: key
    id: AssetOverrides
  - type: format
    id: opents-ini
  - type: format
    id: mix
---

An SHP file opens with a short header carrying the frame count and the logical width and height every frame is placed within, followed by one record per frame and then the frame data itself. Each record holds that frame's offset inside the logical box, its own width and height, the position of its pixels in the file, one color standing in for the whole frame so the radar can draw a cell without examining it, and two flags marking whether the frame carries transparent pixels and whether its pixels are run length encoded.

Non-voxel object art, animations, cursors and interface graphics all use SHP data. Cursors and interface graphics are asked for by fixed names written into the engine. Object art is named from the type's [Image ID](/keys/image/) plus `.SHP`, and two art settings change that name before the file is looked up. [`Theater=yes`](/keys/theater/) replaces the extension with the theater's own — `.TEM` in temperate and `.SNO` in snow — and leaves the rest of the name alone. Failing that, [`NewTheater=yes`](/keys/newtheater/) keeps the `.SHP` extension and rewrites the second letter of the name instead, to `T` in temperate and `A` in snow, but only where the first two letters are `GA`, `NA`, `GT`, `NT`, `CA` or `CT`. A name that starts with anything else is left as written even with the setting on, and a type carrying both settings takes the first.

Whichever name is arrived at is fetched the same way as the fixed names: a loose file of that name in the searched folders first, and otherwise a member of a cached archive; [MIX archives](/formats/mix/) covers which archives those are. A type whose artwork is not found is left with no image rather than with a placeholder.

## Loose files

While [`AssetOverrides`](/keys/assetoverrides/) is on, a shape name is first looked for as a file in [the order files are searched for in](/formats/opents-ini/#the-order-files-are-searched-for-in), and the first copy found is read and kept for the rest of the run. The archives answer only when no folder holds the name. A file too short to hold its header and frame records is passed over as though it were not there. Nothing checks the frame data beyond that, so a loose file is drawn as it is written.

A loose file is given a scale, which is 1 unless one of the following applies, in this order:

1. its logical width and height are both exactly twice those of the archived shape of the same name, which makes it 2;
2. an `HDPACK.INI` in the folder it was found in names it under `[Scale]`, which replaces whatever the comparison gave.

```ini title="HD\HDPACK.INI"
[Scale]
SIDE1.SHP=2
```

The scale is recorded with the shape and does not change how it is drawn on its own. A theater or a side that mounts and drops its own archives changes nothing here: a loose file is found regardless of what is mounted, and is not released when a theater's archives are.

For a BuildingType, `Image=` in `art.ini [<Image ID>]` selects the basename of the main SHP. It does not change the building's Image ID or the section used by other building art keys.
