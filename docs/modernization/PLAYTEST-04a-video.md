# Play-test checklist — 04a Native-resolution video playback

Build: `fork/video-playback`. Needs a retail install under `Run/`. Run the Debug build with
`-WIN` for the first passes so the debug log is readable, then a borderless full screen pass
at 4K. The log line `Video: playing "<path>" WxH at F fps` appears when the new path takes a
movie; `Video: "<path>" did not open; the VQA plays instead` appears when it falls back.

No `SUN.INI` setting turns the path on. A movie plays through it when `<name>.mp4` exists in
the user directory, the game directory, or a folder named in `OPENTS.INI [Paths] SearchPaths`
(for example `SearchPaths=HD,INI,MIX,Maps` with the file at `Run\HD\GDI1.mp4`).

## Making a test file

Extract `GDI1.VQA` from `MOVIES01.MIX` (any MIX tool, or XCC Mixer), then re-encode it:

```
ffmpeg -i GDI1.VQA -vsync 0 -c:v libx264 -pix_fmt yuv420p -profile:v high -crf 18 -c:a aac -b:a 160k -movflags +faststart GDI1.mp4
```

`-vsync 0` keeps the native frame count. For a 4K test add `-vf "scale=3840:2400:flags=lanczos"`
before `-c:a`. For a silent file add `-an` and drop the `-c:a`/`-b:a` pair. ffmpeg 5.1 or later
decodes the version 3 VQAs; check the source with `ffprobe GDI1.VQA` (expect 640x400, 15 fps).

`GDI1` is the first GDI mission briefing. Start a new GDI campaign to reach it; the same file
renamed `WWLOGO.mp4` in the same folder plays on every start, which is the quickest loop.
`EVA.mp4` only plays on a first-time-install start.

## Routing
- [ ] With `GDI1.mp4` beside the game, starting the GDI campaign plays the MP4 (log line above) and the mission loads afterwards exactly as with the VQA.
- [ ] Rename the file `GDI1.MP4` and `gdi1.mp4`: both play.
- [ ] Move the file into `Run\HD\` with `SearchPaths=HD,INI,MIX,Maps` in `OPENTS.INI`: it plays. Remove `HD` from the list: the VQA plays.
- [ ] `WWLOGO.mp4`, `FS_TITLE.mp4` or `STARTUP.mp4` beside the game route the startup sequence through the new path; `TS_Title.mp4` routes the title screen movie.
- [ ] Win a mission with `<WinMovie>.mp4` present, lose one with `<LoseMovie>.mp4` present (`Win=`/`Lose=` in the mission's INI name them), and finish a campaign with `<FinalMovie>.mp4` present: each plays through the new path and the flow after it (score screen, map selection, retry dialog, credits) is unchanged.
- [ ] Delete the MP4: the VQA plays. Replace it with a text file renamed `.mp4`: the log reports the fallback and the VQA plays, in the same request, with no delay beyond a moment.
- [ ] Skirmish with `PlayMovies=no` under `[Session]` in the launch INI, with MP4 files present: no movie of either kind plays.
- [ ] An in-mission EVA radar movie, the main menu animations and the world map clips still play as VQA with MP4 files of their names present.

## Picture
- [ ] Borderless full screen on the 4K display with `ScreenWidth=1920 ScreenHeight=1080`: the 4K MP4 fills the whole window with no border and every pixel of the source is visible (a 1-pixel test pattern stays sharp; the game frame's own scaling does not apply).
- [ ] The same in a 1280x720 window: the movie shrinks to the window, centered, keeping its shape.
- [ ] `StretchMovies=no`: a 1920x1080 MP4 in a 3840x2160 window shows at exactly twice its size (fills it); a 1280x720 MP4 shows at twice its size with a black border; the same files with `StretchMovies=yes` grow to the window's edge.
- [ ] A 640x400 re-encode (no scale filter) shows at 5x in a 3840x2160 window with `StretchMovies=no` (3200x2000, bordered) and at 3456x2160 with `yes`.
- [ ] Resize the window while the movie plays (windowed mode): the picture follows the new size on the next frame.
- [ ] Alt-tab away and back mid-movie: the picture and sound stop together and resume together where they stopped; no stale game frame flashes over the movie, and the game screen is drawn correctly after the movie ends.
- [ ] After the movie the mouse pointer is back and the screen behind it is clean (no leftover movie frame under menus or the loading screen).

## Sound and sync
- [ ] Sound starts within the first frame and lip sync at the start matches the VQA's.
- [ ] Let a long movie run (`INTRO.mp4`, or any 3 minute re-encode) to the 3 minute mark: lip sync has not drifted.
- [ ] A silent re-encode (`-an`) plays at the right speed and ends when the picture ends.
- [ ] A re-encode with 44.1 kHz audio (`-ar 44100`) plays at the right pitch and speed.
- [ ] Lower the movie volume in the audio options: the MP4 sound follows the same setting a VQA's does.
- [ ] Unplug or switch the audio device mid-movie: the picture keeps moving and the movie still ends.

## Skip
- [ ] ESC ends the movie at once in a single player game; the game continues as after a VQA skip. Any other key does nothing.
- [ ] LAN co-op with a second machine: ESC on one machine shows the vote box over the movie (grown with the picture, readable at 4K) on both, and the movie ends only when both have pressed ESC. A machine without the MP4 plays the VQA and still votes and ends together with the other.
- [ ] In the LAN game, alt-tab away on one machine: its movie keeps playing (network games do not pause) and the two machines still end together.

## Not affected
- [ ] With no MP4 files present anywhere, every movie plays as VQA and nothing in the log mentions video files.
- [ ] Save, load, replays and the options dialogs are unchanged.
