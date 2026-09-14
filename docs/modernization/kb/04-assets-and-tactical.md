# In-game assets and the tactical draw pipeline

Commit `0281b88`; re-grep. Read `00-project-overview.md` "The frame" and
`05-file-resolution.md` first.

## 1. Geometry constants

`code/sun.h:84-94`: `CELL_PIXEL_W 24, CELL_PIXEL_H 48` (barely used), `CELL_LEPTON 256`,
`LEPTON_TO_PIXEL(l) ((l)/7)`, `PIXEL_TO_LEPTON(p) ((p)*7)`.
`code/globals.cpp:84-95` (extern `globals.h:287-291`): `ISO_TILE_SIZE = sqrt(34²·2) ≈
48.08`, `ISO_TILE_PIXEL_W = 48`, `ISO_TILE_PIXEL_H = 24`, `LEVEL_LEPTON_H = 104`,
`LEVEL_PIXEL_H = 12`, `CELL_LEPTON_DIAG ≈ 362`.
The iso-tile rasteriser has its own literals: `ISO_WIDTH=48, ISO_HEIGHT=24`
`code/isotype.cpp:57-61`, diamond mask `:1644-1667`, span tables `:1602-1607`.

Projection: `Tactical::Rectangular_To_Isometric` `code/tactical.cpp:187-195` (and int
form `:1769`), `Coord_To_Pixel_Absolute :207-215`, `Z_Lepton_To_Pixel :253` (**`static
double pixels_per_lepton` computed once**, `:259-260`), `Isometric_To_Rectangular :1788` and
`Cell_To_Rectangular :1800` **divide by literal `576`** (= 24·12·2). Matrices
`code/tactical.h:395-405` (only `PixelToCoordMatrix` live, via `Pixel_To_Lepton :239`).

## 2. Draw pipeline

`Tactical::Render(Surface&, bool fullredraw, int drawpass)` `code/tactical.cpp:1068`;
passes `code/tactical.h:35-40`; sub-passes `:172-183` (`Wipe_Depth, Render_Tiles,
Render_Tile_Shadows, Render_Overlays, Render_Fogged_Objects, Render_Shroud,
Render_Buildings, Render_Terrain, Render_Outside_Map`). Pan optimisation swaps
`CompositeSurface ↔ TileSurface` `:1163-1172` and ring-pans `DepthBuffer`/`AlphaBuffer`
`:1158-1162`. Z/A buffers sized to `TacticalRect` (`code/display.cpp:378, :388`; `ZBuffer`
`code/zbuffer.h`, `ABuffer` `code/abuffer.cpp:38`).

Single shape entry point (no `CC_Draw_Shape`):
```cpp
void Draw_Shape(Surface&, ConvertClass&, ShapeSet const*, int shapenum, Point2D const& point,
                Rect const& window, ShapeFlags_Type flags = SHAPE_NORMAL,
                unsigned char const* remap = NULL, int height_offset = 0,
                ZGradientType zgrad = ZGRAD_GROUND, int intensity = 1000,
                ShapeSet const* z_shapefile = NULL, int z_shapenum = 0, Point2D z_off = {});  // code/draw.h:38
```
`code/draw.cpp:76-155`: wraps the frame in a 1-bpp `BSurface` `:89`, `RLE_Blit :142` or
`Bit_Blit :148`; **destination rect = source rect, no scale parameter**. `Blit_Block
:170-208` same. Flags `code/draw.hh:19-40` (`SHAPE_DARKEN` = shadow, translucency,
`SHAPE_REMAP`, `SHAPE_CENTER`, `SHAPE_ALPHA`, `SHAPE_Z*`).

Objects: `TechnoClass::Techno_Draw_Object(...)` `code/techno.cpp:5951` has an `int scale`
("24.8 fixed point") **that the body never reads** (`:5952-6112`); all callers pass 256.
Draws body `:6076` and shadow `:6082` (`shapenum + Get_Count()/2`, `SHAPE_DARKEN`) —
**shadow frames are the second half of the same SHP**. `BuildingClass::Draw_It
code/building.cpp:809`, `InfantryClass code/infantry.cpp:627`, `UnitClass code/unit.cpp:
2911`, `AircraftClass code/aircraft.cpp:400`, `AnimClass code/anim.cpp:496`,
`TerrainClass code/terrain.cpp:394`; overlays via `Tactical::Draw_Overlays
code/tactical.cpp:2085`; `IsometricTileClass::Draw_It` is empty (`code/isotile.cpp:260`)
— tiles are drawn by the tactical passes.

## 3. Colour pipeline (the hard constraint)

The tactical surface is 16-bit 565, not 8-bit. Sprites are palette indices translated
per pixel inside the blitter: every `blitblit.h` template reads `unsigned char color =
*source` and indexes a `ConvertClass` table (`code/blitblit.h:128, 164, 279, 317`).
`ConvertClass` `code/convert.h:60+`: `Translator` (256×16-bit) and `IntensityTranslator`
(63 levels × 256, `NUM_INTENSITY_LEVELS 63 :39`), ~60 pre-built blitters per drawer
`:110-270`. Global drawers `code/_convert.cpp:39-48` (`CameoDrawer, VoxelDrawer,
TerrainDrawer, TiberiumDrawer, AnimDrawer, NormalDrawer, MouseDrawer, SidebarDrawer,
IsometricDrawer`, plus per-tile `TileDrawers`). Per-house remap
`ColorSchemes[House->Scheme]->Converter` (`code/techno.cpp:6007`); per-cell lighting
`LightConvertClass` (`code/lightcon.h:22`); alpha lighting `AlphaLightingRemapClass`
`code/alphalighting.h:25` (`Buffer[256][256]`).

**Consequence:** HD art must be 8-bit indexed on the stock palettes (`UNIT<theater>.PAL`,
`ISO<theater>.PAL`, `ANIM.PAL`, `SIDEBAR.PAL`, `MOUSE.PAL`, cameo palette). 32-bit HD
sprites would need new blitters and a wider surface — out of scope. Remap (house-colour)
pixels are palette indices 16–31 in the unit palette; an upscaler must preserve them.

## 4. Voxels

**Changed by 05a on `fork/voxel-scale`.** The rest of this section describes the
rasteriser as of `0281b88`; the differences are listed at the end of it.

Software rasteriser into a **fixed 256×256×8-bit buffer** (`VOXEL_BITMAP_WIDTH/HEIGHT`
`code/voxdrsys.h:31-33`; buffers `voxdrsys.cpp:25-29`; region centred `:426-430` — a
projected model over ~248 px wraps around the buffer). `VoxelDrawSystem::Render`
`voxdrsys.cpp:369`; `Prep_For_Object :248`, `Prep_For_Shadow :204`,
`Precalculate_Light :143/:163`.
`VoxelLibrary::Render_Object` `code/voxlib.cpp:765` builds an 8.8 fixed-point transform
with a `+128` centring bias `:795-810`; **twelve** drawers from `:1037`, of which
`tests/voxeldraw` pins six. Scale comes from the VXL
(`LayerInfoStruct::Scale` `code/voxlib.h:66`, applied `objtype.cpp:548, :569`). Camera
`Init_Voxel_Matrices` `code/voxel.cpp:210-212` (`Rotate_X(-60°); Rotate_Z(-45°)`, no
scale); `VoxelCameraMatrix` (`code/_voxel.cpp:22`) is identity — **the natural place to
inject a global voxel scale**. Call sites `code/techno.cpp:6123` (`Draw_Voxel`), `:6218`
(shadow), `:6256` (`Techno_Render_Voxel_Object`, ends with `Blit_Block` at `:6303`),
`code/unit.cpp:2512`, `code/bullet.cpp:1555`, `code/vanim.cpp:215/230`.
Cache: `VoxelStaticBuffer(2000000)` `code/_voxel.cpp:23` — a fixed 2 MB arena of RLE
images keyed by facing/frame (`VoxelIndexClass`, `_voxel.h:56`), `UseVoxelCache`
(`init.cpp:2644`), overflow resets the index `techno.cpp:6195-6200`. 2× voxels ≈ 4×
bytes. Any change to `voxlib.cpp` drawers must keep `tests/voxeldraw` green or update the
goldens deliberately.

What 05a changed, and what it left alone:

- `code/voxelscale.h/.cpp` owns the scale. `Set_Voxel_Scale(1|2)` is called from
  `startup.cpp` beside `UIScale`, and `VoxelDrawSystem::Init()` builds the surfaces from
  `init.cpp` after the VPL is loaded. The scale is fixed for the process.
- `VOXEL_BITMAP_WIDTH/HEIGHT` are gone. The bitmap is a fixed 512-square array with
  padding past its last row, described by a `BSurface` built at the chosen size.
- `VoxelFuncArgumentStruct::TransformMatrix` is `Vector3i`, not `Vector3i16`: the 8.8
  screen position needs more than eight whole bits at 2×. The drawers index through
  `Voxel_Buffer_Index(x, y, mask, shift)`, which reduces to the old `(x >> 8) | (y &
  0xFF00)` at scale 1, and lay a voxel down as a `2 * scale` by `scale` block.
- `VoxelPixelDeltaTable` is `int[256][2]`. Its 256 is the RLE skip byte's domain, **not**
  bitmap geometry, so it does not follow the scale.
- `VoxelCameraMatrix` carries `Scale(s, s, 1)`. View space Z is left alone because depth
  lands in an 8-bit buffer with a fixed `+128` bias, and the `.K` row of the transform
  keeps that literal.
- `Prep_For_Shadow` scales `VoxelShadowLightVector` as it applies it, since the light is
  added after the camera transform.
- `[Video] VoxelSupersample=yes` renders at 2× and reduces each 2×2 block to one palette
  index (`code/voxeldownsample.h/.cpp`) before `Get_Surface()` and the returned region are
  handed back, so every caller still works at 1× and the cache still stores 1× images.
  05b turns that reduction off and blits the 2× region instead.
- Untouched and still true for 05b: the cache arena size, the cache keys, `Blit_Block`,
  `Calculate_Sinking_Offset`, and `unit.cpp`'s turret composite at `Point2D(80, 80)`.

## 5. Formats and loaders

- SHP: `code/shapeset.h` — header `Flags/Width/Height/Count` (`:99-110`) + `ShapeRecord`
  (`X, Y, Width, Height, Flags, Size, Color[3], Unused[5], Data` `:116-169`; `short`
  fields, so 2× frames fit; `Color[3]` is the radar colour; `SFLAG_TRANSPARENT 0x01`,
  `SFLAG_RLE 0x02`). In-place cast; no parser. RLE `code/rle.cpp`, LCW `code/lcw.cpp`.
- TMP iso tiles: `IsoTileRecord` / `IsoTileSet` `code/isotype.h:31-103` (per-cell X/Y,
  extra image, **Z data**, height, ramp type, low/high colour). Loaded via `CCFileClass`
  `code/isotype.cpp:1289-1307`; discovery `:1058-1085`; theater suffix `.TEM/.SNO/.URB`
  (`TheaterClass::Suffix`, `code/theater.h:22-70`, consumers `isotype.cpp:681, :708,
  :1054, :1072`, `objtype.cpp:590`, `builtype.cpp:616/629`, `overtype.cpp:302/444/567`,
  `smudtype.cpp:175/285`, `terrtype.cpp:189`, `vein.cpp:399`, palettes `init.cpp:6276`).
- VXL/HVA: `code/voxlib.h/.cpp` (`VoxelLibrary`, `MotionLibrary`), loaded via
  `CCFileClass` `code/objtype.cpp:532-570`.
- PAL: `code/palette.cpp`, `code/colorops.cpp`. PCX: `code/pcx.cpp`.
- `code/sprite.cpp` is one unused 8-bit `Scale_Rotate`.
- `code/srfcache.cpp` caches PCX/BMP dialog images only — not sprites.

## 6. What "zoom" and "scale mode" do today

`Tactical::ZoomFactor` is a post-hoc crop-and-stretch of the finished frame
(`code/gscreen.cpp:523-539`); it magnifies, it does not add detail. `ScaleMode`
(`bgfxbackend.cpp:508-528`) is a whole-frame post filter. `ScreenWidth/Height` is the
internal render resolution — the engine already draws a native 1440p/4K frame; the art is
small, not blurry. That is what an asset-scale mode fixes.

## 7. Blockers for an `AssetScale=2` mode

| # | Blocker | Location |
| --- | --- | --- |
| 1 | `Draw_Shape` has no scale; 1 src px → 1 dst px | `code/draw.cpp:76-155` |
| 2 | `Techno_Draw_Object` scale arg ignored | `code/techno.cpp:5951-6112` |
| 3 | SHP art via `MFCD::Retrieve` — loose HD files invisible | `ObjectTypeClass::Fetch_Normal_Image` `code/objtype.cpp:584` + 121 sites (fixed by prompt 02) |
| 4 | Iso-tile rasteriser literal 48×24 mask and span tables | `code/isotype.cpp:57-61, :1602-1607, :1644-1667` |
| 5 | Inverse projection literal `576` | `code/tactical.cpp:1790-1791, :1804-1805` |
| 6 | `Z_Lepton_To_Pixel` statics | `code/tactical.cpp:259-260` |
| 7 | ~~Voxel buffer 256×256, `+128` bias~~ — done by 05a; `VoxelScale` drives both | `code/voxelscale.h` |
| 8 | Voxel cache 2 MB — still 1× sized, because 05a stores reduced images | `code/_voxel.cpp:23` |
| 9 | Blitters are 8-bit-index in, 565 out | `code/blitblit.h`; `dsurface.h:129` |
| 10 | Shadow frames = second half of the SHP | `code/techno.cpp:6082` |
| 11 | Many pixel offsets in art.ini / code assume 1× (e.g. `PixelSelectionBracketDelta`, cameo, muzzle flash `PrimaryFireFLH` are leptons — fine; but `Draw_It` local pixel nudges are not) | grep `Point2D(` literals in `*.cpp Draw_It` bodies |

Lowest-friction design (prompt 05b): a global `AssetScale` (1 or 2) that multiplies
`ISO_TILE_PIXEL_W/H`, `LEVEL_PIXEL_H`, `LEPTON_TO_PIXEL`, the `576` literal, and the
tile mask; every `ShapeSet` gets an associated scale (1 or 2) recorded by the override
loader; `Draw_Shape` draws HD sets 1:1 and legacy sets through a **new 2× nearest
blitter** so a partial pack stays playable; voxels get `AssetScale` injected into
`VoxelCameraMatrix` with a bigger raster buffer and arena; terrain last, because TMP
carries Z data and extras that must be upscaled coherently.

## 8. Tests

Harness pattern in `00-project-overview.md`. Directly relevant: `tests/voxeldraw`
(golden vectors, "Needs no game data"), `tests/colorops`, `tests/gamedirs` (the search-
order contract — extend it for the HD folder), `tests/shapefacing`, `tests/zbufring`,
`tests/lcwcomp`. Synthesize SHP/TMP bytes in the harness; never read `Run/`.
