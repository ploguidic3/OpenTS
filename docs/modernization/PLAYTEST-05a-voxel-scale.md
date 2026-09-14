# Play-test checklist — 05a Voxel render scale

Build: `fork/voxel-scale`. Needs a retail install under `Run/`. Run the Debug build so the
debug log is readable; the line `VoxelSupersample is ON` or `OFF` appears once with the rest
of the settings. `VoxelSupersample=` goes under `[Video]` in `SUN.INI`; delete the line or
set `no` for the stock drawing. The setting is read at startup only, so each pass needs a
relaunch.

Take every screenshot pair from the same saved game at the same scroll position, with the
existing screenshot key, so an off and an on shot can be compared pixel for pixel.

## What has been run

Nothing on Windows yet. The 144 recorded drawer vectors and the new scale and reduction
cases pass, and CI builds Debug and Release — a build is not runtime evidence, and no part of
this list has been observed in the game.

## Nothing changes with the setting off
- [ ] With `VoxelSupersample=no`, a screenshot of a scene with vehicles, shadows, projectiles
  and voxel debris is identical to the same scene on `fork/video-playback`. Any difference at
  all is a bug: the drawing path is meant to be unchanged.
- [ ] Frame rate on a busy map is unchanged.

## Shape and alignment with the setting on
- [ ] Titan, Wolverine, harvester and Orca at 2560×1440: each is the same size and sits in the
  same place on screen as with the setting off. A unit that has moved by half a cell is not a
  pixel off from where it was.
- [ ] A turreted vehicle's turret sits on its hull exactly as before, and a barrel sits in its
  turret, at all 32 facings. Rotate a Titan and a Mammoth Mk II slowly through a full turn and
  watch for the turret separating from the hull by a pixel at any facing.
- [ ] Shadows stay under their vehicles at every facing and are not offset or doubled.
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
