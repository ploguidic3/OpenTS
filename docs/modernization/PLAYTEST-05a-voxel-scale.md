# Play-test checklist — 05a Voxel render scale

Build: `fork/voxel-scale`. Needs a retail install under `Run/`. Run the Debug build so the
debug log is readable; the line `VoxelSupersample is ON` or `OFF` appears once with the rest
of the settings. `VoxelSupersample=` goes under `[Video]` in `SUN.INI`; delete the line or
set `no` for the stock drawing. The setting is read at startup only, so each pass needs a
relaunch.

Take every screenshot pair from the same saved game at the same scroll position, using the
`ScreenCapture` command, so an off and an on shot can be compared pixel for pixel. It writes
`SCRNnnnn.pcx` at the frame's own resolution (`code/init.cpp:4862`) and its binding is in
`KEYBOARD.INI [Hotkey]`. An external screenshot tool that resizes the frame destroys the
difference this list is looking for.

## What has been run

On Windows, 14 September 2026:

- The setting was compared on and off in matched scenes and judged a slight improvement in
  image quality. The feature is worth keeping on that basis.
- A turreted vehicle was rotated through a full turn with the setting on. The turret stayed
  on its hull and the shadow stayed under the vehicle at every facing, so the reduction grid
  is phase-locked across separately rendered sub-objects as intended.

House colours and the frame rate cost are still unobserved. The comparison against the
unchanged drawing path is covered by test rather than by screenshot; see below.

## Nothing changes with the setting off

A whole-frame screenshot comparison cannot settle this. The frame always carries motion the
setting has nothing to do with — animated shroud edges, tiberium, infantry idle poses — so two
captures of one scene differ on a single build, let alone across two. The check was dropped
for the two below.

- [x] The drawers are unchanged at scale 1. `tests/voxeldraw` hashes the whole bitmap against
  144 vectors recorded before the change, every CI run.
- [x] The centring and region arithmetic is unchanged at scale 1. `tests/voxeldraw` checks
  `Voxel_Region` against the original expression, written out rather than rearranged.
- [ ] Frame rate on a busy map is unchanged.

To compare in the game anyway, crop to one parked vehicle rather than diffing whole frames.
Stand it on plain ground away from tiberium, shroud edge and infantry, deselect it, and
capture. Its pose is served from the voxel cache, so the crop is stable between frames.
Establish that first by diffing two captures on one build; only then is a cross-build diff of
the same crop worth reading.

## Shape and alignment with the setting on
- [ ] Titan, Wolverine, harvester and Orca at 2560×1440: each is the same size and sits in the
  same place on screen as with the setting off. A unit that has moved by half a cell is not a
  pixel off from where it was.
- [x] A turreted vehicle's turret sits on its hull exactly as before, and a barrel sits in its
  turret, at all 32 facings. Rotate a Titan and a Mammoth Mk II slowly through a full turn and
  watch for the turret separating from the hull by a pixel at any facing.
- [x] Shadows stay under their vehicles at every facing and are not offset or doubled.
- [ ] Mammoth Mk II, the largest model, is not clipped at any facing, and is occluded by
  terrain and buildings as before.
- [ ] A unit sinking into water sinks by the same amount and at the same rate.
- [ ] Units on ramps, on bridges, in tunnels and under bridges draw in the right place.
- [ ] Voxel projectiles and voxel debris draw and move as before.

## What should look better
- [ ] Shading across a hull is smoother: the bands that step across a curved surface are
  narrower or gone. Compare a Titan's legs and a harvester's tank at 4K.
- [ ] An outline stops crawling as a unit drives across the screen or turns on the spot. The
  outline is still hard edged; that is expected.
- [ ] The effect does not shimmer between frames. Watch one unit drive a long straight line at
  4K and look for the whole vehicle flickering between two appearances.

## House colours
- [ ] Every side's colour is the same colour it is with the setting off, on every unit type.
- [ ] House colour does not spread onto the hull around its edges, and a thin house coloured
  stripe does not disappear.
- [ ] A unit changing owner, and a unit under the effect of a chronosphere or an EMP, keeps
  its colours correct.

## Cost
- [ ] Frame rate with roughly 100 units on screen at 2560×1440 and at 3840×2160, on and off,
  recorded as numbers rather than an impression.
- [ ] The first moments after a large force comes into view are not noticeably rougher than
  with the setting off, since each new pose is drawn once and kept.
- [ ] Peak working set at scenario load, on and off.
