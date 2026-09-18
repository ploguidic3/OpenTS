# Revisit: depth sorting at AssetScale=2

At `AssetScale=2` an object standing against a building draws over it instead of behind
it. Infantry and a deployed Juggernaut placed on a construction yard sit on its roof. The
rest of the mode is coherent: terrain, cliffs, shroud, fog, lighting and building
footprints all draw at the scale, and `AssetScale=1` is unchanged throughout.

## How depth works here

A depth is `ZBUFFER_MAX` (`0x8000`) less the rows scrolled past and the row itself
(`ZBuffer::Get_Scroll_Delta`), so values sit mid range in the `unsigned short` they are
stored in. The blitters keep a pixel when its depth is at or below what the buffer holds,
so a smaller depth wins and an object lower on the screen is in front.

A shape gets its per-pixel depth from one of two places:

- With a depth shape, `current_z` is constant down the blit and every pixel's depth comes
  from the shape's own bytes. Buildings work this way, through `BUILDNGZ.SHP`.
- Without one, depth comes from the `ZGradStruct` ramp, seeded from the bottom row of the
  clipped destination rectangle and stepped per row. Units work this way.

Only `RLEBlitter::Blit` takes a depth shape. `Blitter::BlitForward` and `BlitBackward`
have no such parameter, so a magnified frame has to stay in the row format to keep its
depth shape; that is what `Scale_Encode_RLE_Frame` exists for.

Depth bytes in artwork -- `BUILDNGZ.SHP`, the per-pixel depth attached to a tile -- are
authored against the 48 by 24 tile and are eight bits wide, so their values cannot be
scaled without overflowing. That is why the buffer was converted to the original tile's
units rather than the artwork scaled up to the view's.

## What was tried, and what each attempt produced

1. Magnified frames drawn through `Bit_Blit`. Buildings lost their depth shape without
   any diagnostic, and fell back to the flat gradient, which writes depth in straight
   lines across the whole bounding rectangle. Anything near a building was cut by a hard
   diagonal edge. Fixed in `6e3f0ce`.
2. Depth shape restored. The clipping went, and objects began losing to buildings
   everywhere instead: the building's depth bytes were classic while the depth they were
   added to counted drawn rows, so the building's surface was flat against a doubled base.
3. Buffer converted to original-tile units, whole expression. Objects began winning
   everywhere. The conversion had divided the `0x8000` bias as well as the row count, so
   every drawn depth moved away from the bias that constants written straight into the
   buffer use.
4. Bias held fixed, only the row count converted (`3e63362`). Objects still draw over
   buildings.

Step 4 not changing the outcome is the useful result: the bias was a real defect but it
is not what decides this, so the remaining cause is in how the two depth sources are
weighed against each other.

## What to try next

Settle it with a measurement rather than another derivation. The base conversion and the
ramp conversion are separable and were changed together, so a temporary `[Video]` key
that switches the ramp between drawn rows and original-tile rows would let one build be
compared in place. One build answers what four CI rounds of one hypothesis each did not.

The candidates worth putting under that test, in order:

- The ramp rate. `z_row_phase` in `blit.cpp` steps the gradient once per original-tile
  row. A unit's depth comes entirely from that ramp and a building's comes entirely from
  its depth shape, so this is the term that sets how they weigh against each other, and it
  was changed in the same commit as the base.
- The building's ground line. `BuildingClass::Draw_It` narrows `cliprect.Height` to the
  drawn half-height of the shape, and the non-top-down depth init seeds from
  `dcliprect.Y + ddrect.Y + ddrect.Height - 1`. If that bottom row is not the building's
  ground line at the larger scale, the building's whole depth surface shifts. This affects
  buildings and not units, which matches the way the defect is asymmetric.
- `set->Height` in the tile rasteriser's `BaseDepth`, which is classic while the `y`
  beside it counts drawn rows. Consistent as written, but never checked against a tile
  whose depth data is known.

## Smaller things left undone

- `Class->YDrawOffset` is added to the screen position in `anim.cpp` without following the
  scale. Its use as a depth offset in the same expressions is correct. Building animations
  may sit a few pixels off at the larger scale.
- The `Get_Z_Adjust` family in `techno.cpp` carries literal depth offsets, including
  `_techno_zadj_rock` and the ramp constants. They are classic, which is now the right
  unit, but none has been checked against real terrain.
- The per-pixel depth attached to a tile is magnified in place but its values are left
  alone, which is correct in the original tile's units and untested against a ramp tile.
- The voxel cache's arena overflow retry calls `Clear_Voxel_Indexes` without rewinding the
  cursor and then adds an index that may be null. This predates the asset scale work and
  was left alone.
