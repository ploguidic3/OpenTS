# Play-test checklist — 04a Native-resolution video playback

Build: `fork/video-playback`. Needs a retail install under `Run/`. Run the Debug build with
`-WIN` for the first passes so the debug log is readable, then a borderless full screen pass
at 4K. The log line `Video: playing "<path>" WxH at F fps` appears when the new path takes a
movie; `Video: "<path>" did not open; the VQA plays instead` appears when it falls back.

No `SUN.INI` setting turns the path on. A movie plays through it when `<name>.mp4` exists in
the user directory, the game directory, or a folder named in `OPENTS.INI [Paths] SearchPaths`
(for example `SearchPaths=HD,INI,MIX,Maps` with the file at `Run\HD\INTR0.mp4`).

## What has been run

Observed on Windows against a Release build of this branch, 13 September 2026. Engine
behaviour under test is that of `0707161`; every later commit touching `code/` adds debug
logging only.

| Group | Result |
| --- | --- |
| Stock path with no container movie present | Plays as VQA throughout, nothing logged about video files |
| A container movie plays | Plays, with sound at 48 kHz stereo; see the encode note below |
| A file that is not a movie | Falls back to the VQA in the same request, no perceptible delay |
| Focus loss and ESC | Picture and sound stop and resume together; ESC ends the movie |
| Name resolution | `INTR0.mp4` is taken for the campaign's opening cinema |
| `StretchMovies` | Both fits confirmed on a 2560x1440 display |

Outstanding: everything the checklist marks that needs a second machine, a long movie, or a
display other than 2560x1440.

## Making a test file

Extract `intr0.vqa` from `MOVIES01.MIX` (XCC Mixer or any MIX tool), then re-encode it.
The retail names are lower case with underscores: `wwlogo.vqa`, `intr0.vqa`, `gdi_m02.vqa` through
`gdi_m12a.vqa`, `nod_m02.vqa` and so on. The `*_sb.vqa` files are the 140x110 radar clips and stay
VQA.

The house-select cinema for mission 1 does not go through `Play_Movie("INTRO.VQA")` directly.
`Choose_Side` (`code/intro.cpp`) first tries a per-disc name, `INTR<n>.VQA` where `n` is the
campaign's CD number, and only falls back to plain `INTRO.VQA` when that file is not present.
For the stock GDI campaign the CD number resolves to 0, so the archive's `INTR0.VQA` is what
actually plays (confirmed 13 Sep 2026); `INTRO.mp4` is never looked at. Numbered mission
movies start at `GDI_M02` for mission 2 onward, so `GDI_M02.mp4` is also the wrong file for a
first-mission test. Use `INTR0.mp4`.

```
ffmpeg -i intr0.vqa -vsync 0 -c:v libx264 -pix_fmt yuv420p -profile:v high -crf 18 -c:a aac -ar 48000 -ac 2 -b:a 160k -movflags +faststart INTR0.mp4
```

`-ar 48000 -ac 2` is required, not optional: the VQA track is mono at 22050 Hz, and an AAC
track kept at that rate plays through Windows' decoder as choppy picture with broken sound.
Resampled to 48 kHz stereo it plays cleanly (observed 13 Sep 2026 with `WWLOGO.mp4`).
`-vsync 0` keeps the native frame count. For a 4K test add `-vf "scale=3840:2400:flags=lanczos"`
before `-c:a`. For a silent file add `-an` and drop the `-c:a`/`-b:a` pair. ffmpeg 5.1 or later
decodes the version 3 VQAs; check the source with `ffprobe intr0.vqa` (expect 640x400, 15 fps).

`INTR0` plays as mission 1's house-select cinema when a new GDI campaign starts, so it doubles
as the first-mission test file. `WWLOGO.mp4` in the same folder plays on every start instead,
which is the quickest loop for the routing and skip passes. `EVA.mp4` only plays on a
first-time-install start.

The debug log carries a `Movie: "<name>" requested; <path or "no matching .mp4 found">` line
for every full screen movie and, for the house-select cinema specifically, a
`Choose_Side(<n>): playing "<name>"` line naming exactly which VQA name was resolved — check
this first if a movie plays as VQA unexpectedly, rather than guessing the file name.

## Routing
- [ ] With `INTR0.mp4` beside the game, starting the GDI campaign plays the MP4 (log line above) and the mission loads afterwards exactly as with the VQA.
- [ ] Rename the file `INTR0.MP4` and `intr0.mp4`: both play.
- [ ] Move the file into `Run\HD\` with `SearchPaths=HD,INI,MIX,Maps` in `OPENTS.INI`: it plays. Remove `HD` from the list: the VQA plays.
- [ ] `WWLOGO.mp4`, `FS_TITLE.mp4` or `STARTUP.mp4` beside the game route the startup sequence through the new path; `TS_Title.mp4` routes the title screen movie.
- [ ] Win a mission with `<WinMovie>.mp4` present, lose one with `<LoseMovie>.mp4` present (`Win=`/`Lose=` in the mission's INI name them), and finish a campaign with `<FinalMovie>.mp4` present: each plays through the new path and the flow after it (score screen, map selection, retry dialog, credits) is unchanged.
- [ ] Delete the MP4: the VQA plays. Replace it with a text file renamed `.mp4`: the log reports the fallback and the VQA plays, in the same request, with no delay beyond a moment.
- [ ] Skirmish with `PlayMovies=no` under `[Session]` in the launch INI, with MP4 files present: no movie of either kind plays.
- [ ] An in-mission EVA radar movie, the main menu animations and the world map clips still play as VQA with MP4 files of their names present.

## Picture

The movie keeps its shape and sits centered. `StretchMovies=yes` scales it by whichever of
width and height runs out first. `no` cuts that factor down to a whole number, and is
ignored where the window is smaller than the movie, since there is no whole number to cut
to. Shape is preserved either way, so a movie shaped unlike the display keeps bars on two
sides under both settings: the 16:10 retail movies never reach the left and right edges of
a 16:9 screen, and reaching them is not what to look for.

From a 640x400 re-encode with no scale filter:

| Display | `StretchMovies=yes` | `StretchMovies=no` |
| --- | --- | --- |
| 2560x1440 | 2304x1440, a bar of 128 at each side | 1920x1200, bordered all round |
| 3840x2160 | 3456x2160, a bar of 192 at each side | 3200x2000, bordered all round |

A 4K re-encode shown on a 2560x1440 display scales below 1, so the whole-number rule does
not apply and both settings give 2304x1440.

- [ ] Both cells of the row for the display to hand, measured rather than judged by eye.
- [ ] Borderless full screen with `ScreenWidth=1920 ScreenHeight=1080` and a re-encode at the display's own height or larger: the picture looks like one clean upscale of the 640x400 source, not the game's own frame buffer resampled a second time on top of ffmpeg's scale. No visible pixel grid, no softness beyond what `lanczos` itself put in. Burned-in text from the original video stays soft; that is the source's native resolution showing, not a defect in this path, and is what prompt 04b's Real-ESRGAN pipeline exists to fix.
- [ ] The same in a 1280x720 window: the movie shrinks to the window, centered, keeping its shape.
- [ ] Resize the window while the movie plays (windowed mode): the picture follows the new size on the next frame.
- [ ] Alt-tab away and back mid-movie: the picture and sound stop together and resume together where they stopped; no stale game frame flashes over the movie, and the game screen is drawn correctly after the movie ends.
- [ ] After the movie the mouse pointer is back and the screen behind it is clean (no leftover movie frame under menus or the loading screen).

## Sound and sync
- [ ] Sound starts within the first frame and lip sync at the start matches the VQA's.
- [ ] Let a long movie run (a 3 minute mission movie such as `GDI_M02.mp4`, reached in mission 2) to the 3 minute mark: lip sync has not drifted.
- [ ] A silent re-encode (`-an`) plays at the right speed and ends when the picture ends.
- [ ] A re-encode with 44.1 kHz stereo audio (`-ar 44100 -ac 2`) plays at the right pitch and speed. A 22050 Hz mono AAC track is known not to; keep every encode at 44.1 or 48 kHz stereo.
- [ ] Lower the movie volume in the audio options: the MP4 sound follows the same setting a VQA's does.
- [ ] Unplug or switch the audio device mid-movie: the picture keeps moving and the movie still ends.

## Skip
- [ ] ESC ends the movie at once in a single player game; the game continues as after a VQA skip. Any other key does nothing.
- [ ] LAN co-op with a second machine: ESC on one machine shows the vote box over the movie (grown with the picture, readable at 4K) on both, and the movie ends only when both have pressed ESC. A machine without the MP4 plays the VQA and still votes and ends together with the other.
- [ ] In the LAN game, alt-tab away on one machine: its movie keeps playing (network games do not pause) and the two machines still end together.

## Not affected
- [ ] With no MP4 files present anywhere, every movie plays as VQA and nothing in the log mentions video files.
- [ ] Save, load, replays and the options dialogs are unchanged.
