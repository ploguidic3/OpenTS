# State: 03b HD interface art

Branch `fork/hud-scale`. The engine side of prompt 03b is done and play-tested; the
pipeline in `tools/hdui` builds a working pack on the user's machine.

## The user's setup

- Retail install: `F:\Games\steamapps\common\Command & Conquer Tiberian Sun`, held in
  `$ts` by the user's PowerShell profile. The game is launched from there as
  `GameD.exe -WIN`; the debug log is written to `$ts\Debug\DEBUG_*.LOG`.
- `OPENTS.INI` in `$ts` sets `SearchPaths=HD,INI,MIX,Maps`; the pack is copied to `$ts\HD`.
- Repository clone: `F:\Games\OpenTS`. `raw\` there holds archives unpacked from the
  install: `CONQUER.MIX`, `LOCAL.MIX` and `CACHE.MIX` from `TIBSUN.MIX`, and `SIDEC01.MIX`.
- `realesrgan-ncnn-vulkan` is installed and named by `OPENTS_REALESRGAN`. The zip carried
  2× weights for `realesr-animevideov3` only, so that is the model in use.
- The install keeps few archives loose. `tools/hdui/README.md` gives the extraction and
  the build command; `SIDEC01.MIX` must come first so the sidebar palette answers.

The pack in use is built with:

```powershell
python tools\hdui\build_hdui_pack.py HD --mix raw\SIDEC01.MIX --mix raw\CACHE.MIX --mix raw\LOCAL.MIX --mix "$ts\TIBSUN.MIX" --mix raw\CONQUER.MIX --model realesr-animevideov3 --work-dir frames
Remove-Item HD\*.FNT
```

## Play-test results

Passed: sidebar with no seams and an unchanged cameo row count; buttons, cameos, scroll
arrows and radar clicks hit where they draw; the radar rebuilds on a resolution change;
the cursor is sharp, the right size in menus and play, and hits at a unit's edge;
dialogs and the main menu are unchanged; `AssetOverrides=no` returns the classic look and
a single save loads either way.

The visible gain is small. The pack covers the frame, buttons, clocks, radar frame, tabs
and cursor; the cameos, which fill most of the sidebar, are not in it.

Not yet run: the Mixed pack removals of `RCLOCK2.SHP`, `DARKEN.SHP` and `SIDE2.SHP`;
`CursorScale=2` against 1; sidebar tooltip placement; the radar view outline and event
flashes. 4K and multiplayer checks need hardware the user does not have.

## Parked

- Fonts. A 2× `.FNT` draws as garbage although `tools/hdui/fntcheck.py` finds it faithful
  to its source and to the header rules `WWFontClass::Print` reads by. The fault is in the
  engine and is untraced. Packs are built and then the fonts deleted.
- Text size. Without the 2× fonts, sidebar and tab text is half the size of the artwork
  around it, because the surfaces are at the pack's scale and nothing magnifies the text
  drawn into them. Routing those prints through `UI_Draw_Scaled` enlarged the text but put
  cameo captions above their cameos; it was reverted (`e074a90`, `f572f36`).
- Depth sorting at `AssetScale=2`, from 05b: `REVISIT-05b-depth-sorting.md`.

## What cost time here

- `fntcheck` and the round-trip tests compared the pipeline's reader with its own writer,
  so a font mislabelled in a way only the engine noticed passed every check. Test output
  against what the engine reads, from the engine source.
- Downscaled screenshots were misread more than once. Ask for the debug log lines or a
  close crop before changing code on what a screenshot seems to show.
