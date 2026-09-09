# File resolution: where an override can live

The single most important fact for UI art, HD art and video: OpenTS has **two** lookup
paths, and only one of them sees loose files.

## Path A — `CCFileClass` (loose files win)

`CCFileClass::Open` (`code/ccfile.cpp:416-479`) checks the disk first — comment at
`:425-431`: "allows upgrade files to work" — and only then asks `MFCD::Offset` for a mix
member. What "on disk" means is `CDFileClass::Set_Name` (`code/cdfile.cpp:319-393`):

1. the user path (`User_Path_For`, `cdfile.cpp:216`; skipped for names containing `\ / :`)
2. the current directory
3. each search folder in order — built by `Init_Search_Folders` (`code/gamedirs.cpp:245`)
   from `OPENTS.INI [Paths] SearchPaths` (`code/deploymentconfig.cpp:28`, default
   `"INI,MIX,Maps"`, `code/deploymentconfig.h:24`), called from `code/startup.cpp:568`
4. the mix archives

`Search_Files(pattern)` (`code/gamedirs.cpp:326`) walks the same order for globs.
A written `SearchPaths` **replaces** the default (`manual/content/formats/opents-ini.md:29`),
so an HD folder is `SearchPaths=HD,INI,MIX,Maps`.

Uses Path A today: INI files, PCX dialog art and title screens (`Read_PCX_File` takes a
`FileClass`), iso tiles (`IsometricTileTypeClass::Load_Tile_Data`, `code/isotype.cpp:1289`,
`CCFileClass`), voxels and HVAs (`code/objtype.cpp:532-570`), movies
(`Play_Movie` → `CCFileClass(name).Is_Available()`, `code/movie.cpp:88`), palettes.

## Path B — `MFCD::Retrieve` (mix only, first mounted wins)

`MixFileClass::Retrieve` (`code/mixfile.cpp:279-284`) → `Offset` (`:523-577`) walks the
mounted list front to back and returns a pointer only into a `Cache()`d archive. There is
**no filesystem fallback**. Mount order (`Init_Bootstrap_Mixfiles`, `code/init.cpp:2361-
2396`): `PATCH.MIX`, `PCACHE.MIX` → `EXPAND99..00.MIX`, `ECACHE99..00.MIX` → `TIBSUN.MIX`
→ `CACHE.MIX` → `LOCAL.MIX`; then `Init_Secondary_Mixfiles` (`:2413`) adds `CONQUER.MIX`,
`MAPS*.MIX`, `MULTI.MIX`, `SOUNDS*.MIX`, `MOVIES*.MIX` (globbed, `:2506-2521`).

Uses Path B: **all SHP art** — `ObjectTypeClass::Fetch_Normal_Image`
(`code/objtype.cpp:584-610`) and 121 `MFCD::Retrieve` call sites across 30 files, including
every HUD asset (`SIDE1/2/3.SHP`, `TABS.SHP`, `RADAR.SHP`, `MOUSE.SHP`, cameos, fonts via
`Load_Alloc_Data` on mix data). The manual says so: `manual/content/formats/shp.md`
("fetched from a cached archive rather than opened as a file") — and
`manual/content/formats/opents-ini.md` currently overstates loose-file coverage.

Existing override precedent, `_DEBUG` only: `MouseClass::One_Time` `code/mouse.cpp:344-362`
tries `RawFileClass("MOUSE.SHP").Is_Available()` → `Load_Alloc_Data` before
`MFCD::Retrieve`. That is the pattern to generalise.

## Consequences

- A loose `.VXL/.HVA/.TEM/.SNO/.URB/.PAL/.PCX/.INI/.MP4` in an `HD` folder already
  overrides. A loose `.SHP` does not; today it must be packed into an `ECACHE00.MIX`
  (mounted before `TIBSUN.MIX`, and `Cache()`d so `Retrieve` sees it).
- The fork's override infrastructure (prompt 02) adds one function — e.g.
  `Fetch_Shape(name)` — that tries `CCFileClass` first and falls back to `MFCD::Retrieve`,
  then migrates call sites to it. Ownership changes: `Retrieve` returns borrowed mix memory;
  a loose load returns an owned buffer that must be kept alive for the process (a static
  registry keyed by name is fine — the mixes are never freed either).
- `ShapeSet` is an in-place cast of the file bytes (`code/shapeset.h:99-110`), so a loose
  SHP loaded with `Load_Alloc_Data` is immediately usable; nothing validates its size.
- `ECACHE`/`EXPAND` mixes remain the distribution format if you ever want to share a pack
  with stock-OpenTS players; the loose folder is for development.
