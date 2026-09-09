# How these prompts are shaped, and how to write more

Every prompt has the same skeleton. If you add a phase, keep it.

1. **Context to load** — `@`-imports of the kb files the phase needs. Never import all
   of them; the tactical/asset file alone is long.
2. **Goal** — one paragraph, player-visible outcome, default-off setting named.
3. **Verify first (plan mode)** — the line-level claims from the kb that the design
   depends on. Claude Code re-greps them and shows a plan before editing. This is the
   step that absorbs drift since commit `0281b88`.
4. **Design** — numbered, with the single-choke-point principle: one place where a
   constant is multiplied, one function that resolves a file, one loop that honours the
   skip/idle contract. Prefer new files over edits to inherited ones.
5. **Tests** — harness in `tests/`, synthetic data only, CTest-registered.
6. **Deliverables** — code, both configurations built, manual change record, key page,
   `ini-keys.yaml`, and a play-test checklist for you (Claude Code cannot run the game).
7. **Report** — files, behaviour classification, exact commands, what was not run.

Phrases that matter to this repo's hook and rules: "A build is not runtime evidence";
"intentionally changed, opt-in"; "no compatibility boundary crossed" (with the grep that
proves it); "documentation updated: … / remains accurate because …".

Session hygiene: `/clear` between prompts; if a prompt overruns context, ask Claude Code
to write `docs/modernization/STATE-<nn>.md` summarising what was done and what remains,
then `/clear` and resume from that file.
