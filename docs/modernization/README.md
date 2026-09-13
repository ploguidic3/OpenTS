# OpenTS modernization kit

A knowledge base and a numbered prompt series for driving Claude Code through four
modernization projects on a personal fork of
[OpenTS](https://github.com/OpenTS-Developers/OpenTS):

1. **Modern controls** — right-click orders, left-click selects and deselects.
2. **Scaled interface** — sidebar, tabs, radar, tooltips and fonts that look right at 1440p/4K.
3. **4K campaign cutscenes** — a new video path in the engine plus an AMD-friendly upscale pipeline.
4. **HD in-game assets** — an `AssetScale=2` engine mode, an override folder, and an upscale pipeline for SHP, voxel and terrain art.

Everything in `kb/` was written from a source survey of OpenTS at commit `0281b88` (9 Sep 2026).
Line numbers are accurate for that commit and will drift; every prompt tells Claude Code to
re-grep before editing. The prompts assume a **personal fork** and a **Radeon RX 9070 XT**
(AMD, so no CUDA) on Windows.

## Install into your fork

```
your-fork/
├── CLAUDE.md               ← OpenTS's own, leave it alone (it imports AGENTS.md)
├── CLAUDE.local.md         ← copy from this kit; git-ignored by convention
└── docs/modernization/     ← copy kb/ and prompts/ here
    ├── kb/
    └── prompts/
```

1. Fork OpenTS, clone with `--recurse-submodules`, and confirm a clean Win32 Release build
   per `docs/BUILDING.md` before touching anything.
2. Copy `CLAUDE.local.md` to the repo root. Claude Code reads it after `CLAUDE.md`, so your
   fork policy and the `@docs/modernization/kb/...` imports load in every session.
3. Copy `kb/` and `prompts/` to `docs/modernization/`. Add `CLAUDE.local.md` to
   `.git/info/exclude` if you don't want it in the fork history; the kb folder can be committed.
4. Put a retail Tiberian Sun install under `Run/` (the repo's `.gitignore` already covers it).

## How to run a prompt

Each file in `prompts/` is one Claude Code session. Open the fork in Claude Code, then:

```
/clear
Read docs/modernization/prompts/01-modern-controls.md and carry it out.
Start in plan mode: read the files it lists, confirm the line references still
hold, and show me the plan before editing.
```

Rules that make this go well:

- **One prompt per session, `/clear` between them.** The prompts are long and the kb files
  are imported automatically; stacking phases in one context degrades results.
- **Always start in plan mode** (`Shift+Tab` or say "plan first"). Every prompt has a
  *Verify first* section that tells Claude Code which line references to re-check.
- **Build between prompts yourself** and play-test. OpenTS's `AGENTS.md` is explicit that a
  build is not runtime evidence; Claude Code cannot run the game.
- Phases marked (a), (b), (c) are separate sessions. They are ordered by dependency.
- When a prompt says "stop and report", it means the design decision is yours: answer, then
  continue in the same session.

## Recommended order

| Order | Prompt | Why here |
| --- | --- | --- |
| 0 | `00-setup.md` | Verifies the build, the test harness, the hooks, and writes a fork-policy note. |
| 1 | `01-modern-controls.md` | Smallest change; exercises the whole edit → build → manual → test loop. |
| 2 | `02-asset-override-infrastructure.md` | Loose-file override for SHP art and an `HD` search folder. Both the UI and asset projects need it. |
| 3 | `03a-hud-integer-scale.md` then `03b-hd-ui-art.md` | Integer HUD scale gives an immediate 1440p win; HD art then replaces the nearest-neighbour look. |
| 4 | `04a-video-playback.md` then `04b-cutscene-pipeline.md` | Engine path first (test it with any MP4), then the upscale pipeline. |
| 5 | `05a-voxel-scale.md`, `05b-asset-scale-mode.md`, `05c-shp-upscale-pipeline.md`, `05d-terrain.md` | Largest project; each step leaves the game playable. |

A phase that builds on the one before it belongs on that phase's branch, not on a branch
chosen before it existed. `04b` writes into `manual/content/formats/video-files.md`, which
`04a` creates, so `04b` goes on `fork/video-playback`. Tell the session so at the start;
a session that is handed a different branch by its harness will otherwise take that one.

## What is in `kb/`

| File | Covers |
| --- | --- |
| `00-project-overview.md` | Repo layout, build, run, tests, hooks, manual obligations, fork policy, the surfaces and the colour pipeline. |
| `01-input-and-controls.md` | Mouse path from `WM_*` to `Active_Click`; every handler with file:line; options and dialog plumbing. |
| `02-rendering-and-ui.md` | bgfx presenter, `VideoScaleInfo`, surfaces, HUD layout constants, fonts, cursor, `docs/UI_DESIGN.md` summary. |
| `03-movies-and-video.md` | VQA player, audio bridge, movie lookup, presentation bottleneck, codec/licence options. |
| `04-assets-and-tactical.md` | Tactical projection constants, `Draw_Shape`, blitters, voxel rasteriser, formats, caches, tests. |
| `05-file-resolution.md` | The `CCFileClass` vs `MFCD::Retrieve` asymmetry that decides where overrides can live. |
| `06-upscale-tooling-amd.md` | The RX 9070 XT pipeline: ffmpeg VQA decode, Real-ESRGAN via Vulkan or ROCm, palette re-quantisation, encoding. |
