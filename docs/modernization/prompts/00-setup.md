# Prompt 00 — Fork setup and baseline

Context to load: `@docs/modernization/kb/00-project-overview.md`,
`@docs/modernization/kb/05-file-resolution.md`.

## Goal

Confirm the fork builds and tests cleanly, that the repo's hooks are active, and record
a baseline so later prompts can prove they changed nothing they did not intend to.

## Do

1. Read `AGENTS.md`, `code/AGENTS.md`, `manual/AGENTS.md`, `CONTRIBUTING.md`,
   `docs/BUILDING.md`, `docs/STYLE.md`. Summarise in ten lines the rules you will follow
   and the two places (`CLAUDE.local.md`) where the fork overrides them.
2. Verify submodules are initialised (`thirdparty/bgfx.cmake`, `thirdparty/miniaudio`
   non-empty). If not, run `git submodule update --init --recursive`.
3. Configure and build Win32 Debug and Release exactly as `docs/BUILDING.md` says. Report
   the exact commands and any new warnings. Do not call a build "runtime evidence".
4. Run CTest for both configurations. Report pass/fail per suite.
5. Confirm `.claude/settings.json` hooks run (`python .agents/hooks/style-rules.py --check`).
6. Confirm `Run/` contains a retail install (look for `TIBSUN.MIX`, `MOVIES01.MIX`,
   `SUN.INI` or the marker file). Never list, copy or commit its contents.
7. Write `docs/modernization/BASELINE.md` recording: commit hash, build outputs and
   sizes, CTest results, ffmpeg/ffprobe version if present on PATH, GPU (RX 9070 XT) and
   driver version if obtainable via `dxdiag /t` or `wmic`.
8. Create the fork's settings scaffold that every later prompt extends, **with no
   behaviour change yet**:
   - In `code/options.h` / `code/options.cpp`, add a clearly delimited block
     `// Fork settings` for future `[Options]`/`[Video]` keys, near `AutoScroll` and
     `CursorScale`. Empty for now.
   - In `manual/changes/`, nothing yet.
9. Build again; confirm no diff in behaviour (the block is empty).

## Stop and report

List anything that failed and the exact error. Do not work around a missing Visual
Studio component or a failed submodule fetch — report it.
