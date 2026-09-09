# Prompt 05a — Voxel render scale (and supersampled voxels at 1×)

Context to load: `@docs/modernization/kb/00-project-overview.md`,
`@docs/modernization/kb/04-assets-and-tactical.md`.

## Goal

Two things, both in the voxel rasteriser and independent of any art pack:
1. Parameterise voxel rendering by an integer `VoxelScale` so units can be rasterised
   at 2× when 05b's `AssetScale=2` is on.
2. At `AssetScale=1`, optionally render at 2× and box-downsample to 1× (`[Video]
   VoxelSupersample=yes`), giving anti-aliased vehicles today with no pack.

## Verify first (plan mode)

- `VOXEL_BITMAP_WIDTH/HEIGHT` 256 in `code/voxdrsys.h`; buffers in `voxdrsys.cpp`;
  `Render()` centres the region.
- `VoxelLibrary::Render_Object` builds the 8.8 transform with a `+128` bias.
- `VoxelCameraMatrix` is identity; `Init_Voxel_Matrices` has no scale term.
- `VoxelStaticBuffer(2000000)`; `UseVoxelCache`; overflow reset in `techno.cpp`.
- `tests/voxeldraw` pins the six drawers with golden vectors.
- `Techno_Render_Voxel_Object` ends in `Blit_Block` at a position derived from the
  region; `Draw_Voxel` computes the screen point from the 1× projection.
Show me the plan and how the golden test stays valid.

## Design

1. `int VoxelScale` (1 or 2) owned by `code/voxdrsys.cpp`, set from `AssetScale` (05b)
   or from `VoxelSupersample`. Bitmap buffers become `256*VoxelScale` square, allocated
   at init (not static arrays), `VoxelPixelDeltaTable` sized accordingly. The `+128`
   centring bias becomes `+128*VoxelScale` expressed through one helper; audit every
   literal in `voxlib.cpp:765-820` and `voxdrsys.cpp` for `128`, `256`, `>> 8`, `& 0xFF`
   and decide which are fixed-point (leave) and which are bitmap geometry (scale).
2. Scale injection: multiply `VoxelCameraMatrix` by `Scale(VoxelScale)` in
   `Init_Voxel_Matrices`/`Set_Voxel_Camera_Angle`; verify shadows use the same path.
3. Supersample at 1×: after `Render()`, if `VoxelScale==2 && AssetScale==1`, box-filter
   the 8-bit region to 1× *in palette space* — pick the majority index of each 2×2 block
   (or nearest palette colour of the RGB average, using the VPL/palette; majority is
   cheaper and avoids remap-range mistakes). Then blit at 1×.
4. Cache: `VoxelStaticBuffer` size `2000000 * VoxelScale²`; `VoxelIndexClass` keys
   unchanged.
5. `tests/voxeldraw`: keep the existing goldens at scale 1 untouched; add a scale-2 case
   that renders the same synthetic voxel and checks the bounding box doubles and pixel
   count roughly quadruples. Add a 2×2 majority-downsample unit test.

## Deliverables

Code; Debug+Release; CTest green; `manual/changes/voxel-supersampling.md`;
`manual/content/keys/voxelsupersample.md` + `ini-keys.yaml`; play-test checklist: Titan,
Wolverine, harvester, Orca at 1440p with `VoxelSupersample=yes` vs `no` — screenshots via
the existing screenshot key; turret/barrel HVA sub-objects aligned; shadows aligned;
Mammoth Mk II (largest model) not clipped; frame rate impact with 100 units on screen.
