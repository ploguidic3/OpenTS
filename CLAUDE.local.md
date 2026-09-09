# Fork instructions (personal modernization fork)

This is a personal fork of OpenTS. The upstream `AGENTS.md` rules still apply, with the
changes below. When they conflict, this file wins.

## Fork policy

- Features here are **intentionally changed** behavior behind opt-in settings. Every new
  behavior is off by default so a stock `SUN.INI` plays exactly like upstream.
- Upstream's "discuss with a maintainer first" rule does not apply. Do not open upstream
  issues or PRs unless asked.
- Upstream forbids AI attribution trailers. In this fork, follow the attribution guidance
  Claude Code's own environment supplies for commit messages; the upstream hook does not
  block it.
- Keep every change rebase-friendly: new code in new files where possible, minimal edits to
  inherited files, no reformatting of untouched code.
- The manual obligations still hold. Each player-visible change gets a
  `manual/changes/*.md` record and a `manual/content/keys/*.md` page. Use `release: fork`
  in the change record front matter unless told otherwise.

## Working method

- Start every prompt from `docs/modernization/prompts/` in plan mode. Re-grep every
  file:line reference in the knowledge base before editing; they were taken at commit
  `0281b88` and drift.
- A build is not runtime evidence. Say exactly what was built and what was not run.
- Never touch `Run/` (retail game data) and never commit anything from it.
- Tests may not read retail assets. Synthesise bytes in the harness, as `tests/voxeldraw`
  and `tests/gamedirs` do.

## Knowledge base

@docs/modernization/kb/00-project-overview.md
@docs/modernization/kb/05-file-resolution.md

The per-project files are imported by the prompts that need them:
`01-input-and-controls.md`, `02-rendering-and-ui.md`, `03-movies-and-video.md`,
`04-assets-and-tactical.md`, `06-upscale-tooling-amd.md`.
