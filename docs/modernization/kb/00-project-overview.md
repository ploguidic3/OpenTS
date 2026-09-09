# OpenTS project overview

Survey of OpenTS at commit `0281b88` (9 Sep 2026). Line numbers drift; re-grep before editing.

## What OpenTS is

A standalone C++ reconstruction of Tiberian Sun 2.03 Firestorm, GPLv3-or-later with EA's
GPL §7 terms on inherited files. It ships the engine only (`Game.exe`, `Language.dll`); the
retail install supplies all data. Release 0.1.0 plays the full game. The renderer is bgfx
and the audio layer is miniaudio. Upstream's declared milestones are CnCNet support,
Vinifera/ts-patches parity, then RA2-level features; UI scaling and HD art are not on that
list, and `docs/UI_DESIGN.md:132` states "No user UI scale setting yet" as a deliberate
limit. That is why this work lives on a fork.

## Repository layout

| Path | Content |
| --- | --- |
| `code/` | ~900 C/C++ files, flat. `_`-prefixed files hold globals for the matching class file (`_surface.cpp`, `_voxel.cpp`). `.hh` files are small definition headers (enums). |
| `code/audio/` | miniaudio-based engine: device, mixer, streams, `audiomovie.cpp` bridge for VQA. |
| `code/vqalib/` | Westwood VQAPlay32 library, separate static lib. |
| `code/language/` | `language.rc` dialog templates and `language.h` control IDs (Win32 dialogs). |
| `code/resources/` | icons. |
| `thirdparty/` | `bgfx.cmake` and `miniaudio` as submodules, LZO vendored, `licenses/`. |
| `tests/` | 39 hand-rolled harnesses, CTest-registered. |
| `manual/` | The user manual: `content/` (keys, formats, systems, commands), `changes/` (lifecycle records), `data/*.yaml` (generated inventories), `schema/`, `tools/`. |
| `docs/` | `BUILDING.md`, `STYLE.md`, `DIRECTION.md`, `UI_DESIGN.md` (proposal), `RATIONALE.md`, `HISTORY.md`. |
| `Run/` | Where the retail game data goes (git-ignored, `place_steam_build_here` marker). |
| `.agents/hooks/style-rules.py`, `.claude/settings.json` | Prose/comment rule hook, runs on SessionStart, PostToolUse (Edit/Write), Stop. Never blocks. |

## Build and run

- Supported: Visual Studio 2022, **Win32 (32-bit)**, Debug and Release, CMake ≥ 3.23, C++20.
  `code/CMakeLists.txt:5-10` hard-errors on 64-bit unless `OPENTS_EXPERIMENTAL_X64=ON`.
  Static CRT `/MT`; `/LARGEADDRESSAWARE` (`code/CMakeLists.txt:232`).
- Configure/build:
  ```powershell
  git submodule update --init --recursive
  cmake -S . -B build -G "Visual Studio 17 2022" -A Win32
  cmake --build build --config Release
  ```
  Output `build/bin/Release/Game.exe`; Debug is `GameD.exe`.
- Run from the build dir against the data dir: `build\bin\Debug\GameD.exe -DATADIR=Run`.
  Saves/logs go to the user dir (defaults to the exe's dir; `-USERDIR=`).
- Windowed mode `-WIN`; resolution `-WxH` (`code/init.cpp:1749`).
- bgfx headers are scoped to `code/bgfxbackend.cpp` only (`code/CMakeLists.txt:158-164`).
  miniaudio device API only in `code/audio/audiodevice_ma.cpp`. Keep new third-party
  includes scoped the same way.
- No shader toolchain: `BGFX_BUILD_TOOLS` is OFF (`thirdparty/CMakeLists.txt:13`); the
  presenter uses bgfx's embedded imgui shaders. A custom shader needs `shaderc` added.

## Tests

`tests/CMakeLists.txt:12-63` defines `opents_add_test(Name NAME dir SOURCES ... ENGINE
a.cpp b.cpp ... INCLUDES ... DEFINITIONS ... [FLOAT] [UTF8] [STAMP])`. A harness compiles
the specific engine `.cpp` files it exercises and stubs the rest; it never links the game.
Style: `Check(bool, "what")` printing `ok`/`FAILED` and a `Failures` counter
(`tests/gamedirs/gamedirscontract.cpp:31-41`). Rule from `AGENTS.md:92`: tests must not
require proprietary assets. Relevant precedents: `tests/gamedirs` (search-order contract,
makes its own files), `tests/voxeldraw` (golden vectors for the voxel rasteriser),
`tests/colorops` (565 colour ops), `tests/ini`, `tests/deploymentconfig`. None touch input.

## Manual obligations (still followed on the fork)

For a player-visible change:
- `manual/changes/<slug>.md` with front matter `title, category (feature|fix|balance|
  performance|internal), release, breaking, migration[], targets[{type,id,effect}],
  credit[]`. Template: `manual/changes/edge-scrolling-option.md`. Schema
  `manual/schema/authored-change.schema.json`.
- `manual/content/keys/<key>.md` with `key, summary, see_also, when_omitted`. Template:
  `manual/content/keys/autoscroll.md`.
- An entry in `manual/data/ini-keys.yaml` (see `CursorScale` at ~`:5920` for a `[Video]`
  key, `ScrollMethod` at ~`:19095` for `[Options]`); `manual/tools/` has `ini_inventory.py`,
  `validate_manual.py`, `manage.py`. Read `manual/AGENTS.md` and `manual/AUTHORING.md`
  before writing there.
- Prose rules from `AGENTS.md` "Writing prose": plain, direct, dense reference prose; no
  meta-commentary. The hook re-injects these rules on Markdown edits.

## Settings files

| File | Read by | Purpose |
| --- | --- | --- |
| `SUN.INI` (`CONFIG_FILE_NAME`, `code/sun.h:69`) | `OptionsClass::Load_Settings` `code/options.cpp:357`, saved `:457` | `[Options]`, `[Video]`, `[Audio]` player settings. |
| `KEYBOARD.INI` | `Init_Hotkeys` `code/init.cpp:5878` | `[Hotkey]` command bindings. |
| `UI.INI` `[Ingame]` | `UIControlsClass::Read_INI` `code/uicontrol.cpp:28` | Vinifera-style presentation flags (action lines etc.). |
| `OPENTS.INI` | `DeploymentConfigClass` `code/deploymentconfig.cpp` | `[Paths] SearchPaths=INI,MIX,Maps`, `[Saves] CarryScenarioFile`. Probed at `""`, `INI\`, `MIX\`. |
| `rules.ini [Movies]` | `RulesClass::Do_Movies` `code/rules.cpp:2071` | Movie name table; index order must match `code/vq.hh`. |

`[Video]` keys today (`code/options.cpp:407-421`): `ScreenWidth, ScreenHeight,
StretchMovies, Fullscreen, WindowWidth, WindowHeight, ScaleMode (PixelArt|Nearest|Linear),
IntegerScaling, VSync, Renderer, CursorScale`. New fork keys go beside these.

## The frame, in one paragraph

The game draws one software frame of 16-bit RGB565 pixels at the *logical* resolution
`Options.ScreenWidth × ScreenHeight` into GDI-DIB `DSurface`s (`code/dsurface.cpp:66-72`,
"16 bit 565 and nothing else"). Sprites are 8-bit palette indices converted per pixel inside
the blitters through `ConvertClass` translate tables (`code/blitblit.h`). `Video_Present`
(`code/video.cpp:263`) uploads the whole `VisibleSurface` as one texture and
`Backend_Present` (`code/bgfxbackend.cpp:473`) draws one quad scaled to the window with
aspect fit, optional integer snap, and a nearest/linear/pixel-art filter
(`Update_Scale_Info` `code/video.cpp:83-112`). The window is per-monitor-DPI aware and the
desktop mode is never changed; fullscreen is a borderless window at desktop size. So at 4K
the frame can already be 3840×2160 — the art is simply small, not blurry. The hardware
cursor is built from `MOUSE.SHP` and is the only element with its own scale (`CursorScale`).

Surfaces (`code/_surface.cpp:14-23`): `VisibleSurface` (presented), `HiddenSurface`,
`CompositeSurface`/`TileSurface` (tactical, swapped on pan), `SidebarSurface`,
`AlternateSurface` (dialogs), `LogicalSurface`. Allocated in `Allocate_Surfaces`
(`code/init.cpp:5942-6024`) and reallocated by `Change_Display_Mode`
(`code/mainopt.cpp:191-305`).

## Dispatch objects you will meet everywhere

- `Map` — the `MouseClass` singleton (`code/_map.cpp:19`); chain `GScreenClass → MapClass →
  DisplayClass → RadarClass → PowerClass → SidebarClass → TabClass → ScrollClass →
  MouseClass`.
- `TacticalMap` — `Tactical*` (`code/_tactica.cpp`), iso view, rubber band, hit tests.
- `Options` — `OptionsClass` global. `Keyboard` — `WWKeyboardClass*`. `MouseCursor` — `Mouse*`.
- `MFCD` — `MixFileClass<CCFileClass>`; `MFCD::Retrieve(name)` returns a RAM pointer into a
  cached mix, or NULL. See `05-file-resolution.md` for why this matters.

## Style essentials (from `docs/STYLE.md` and `code/AGENTS.md`)

Follow surrounding naming (`Snake_Case_Functions`, `PascalCase` members, `Is`/`Has`
booleans). C++20 in new files. Comments sparse, one sentence on what the code cannot show,
never describing the edit. Keep historical headers untouched. New files carry the
"Copyright 2026 OpenTS contributors" header form used by `code/movies.cpp:1-8`, not the EA
header.
