# Prompt 04a — Native-resolution video playback path

Context to load: `@docs/modernization/kb/00-project-overview.md`,
`@docs/modernization/kb/03-movies-and-video.md`, `@docs/modernization/kb/05-file-resolution.md`.

## Goal

When a movie is requested and a `<name>.mp4` exists (loose file, `HD` folder, or beside
the mixes), play it through a new decoder → bgfx path at the movie's native resolution,
fitted to the **window's drawable size**, bypassing the 565 game frame and the GDI
`StretchBlt`. Otherwise play the VQA exactly as today. Audio, ESC skip, the network skip
vote, and every caller's blocking contract are preserved.

## Decision to confirm before planning

Decoder: **Windows Media Foundation Source Reader** (no new dependency, hardware
decode on the RX 9070 XT, H.264/AAC MP4). Alternative is dav1d + IVF (portable, needs
vcpkg). I choose Media Foundation for this fork unless you object in your plan; state the
trade-off in one paragraph and proceed.

## Verify first (plan mode)

- `Play_Movie(char const*)` in `code/movie.cpp` is the funnel; `Play_Movie(VQType)`
  appends `.VQA` into a 20-byte static buffer; `Movie_Create/Play/Destroy` in
  `code/movies.cpp`; `VQHandle` in `code/movie.h`.
- `MovieSkip::Playback`/`Idle`/`Draw_Overlay` in `code/movieskip.*`.
- `MovieSinkClass` and `Stream_Audio_Handler` in `code/audio/audiomovie.cpp`; the
  push-stream producer API in `code/audio/audiostream.h`.
- `Backend_Present` and the frame texture in `code/bgfxbackend.cpp`; `VideoScaleInfo`
  and `Update_Scale_Info` in `code/video.cpp`; `Win_Window_Drawable_Size` in
  `code/winstub.cpp`.
- `code/CMakeLists.txt` links; bgfx include scoping.
Show the plan with the new files and the three touched inherited files.

## Design

1. `code/video/videoplayer.h` (fork): an abstract `VideoPlayer` with `Open(name)`,
   `Width/Height/FrameRate`, `Start()`, `bool Present_Frame(double audio_seconds)`,
   `Feed_Audio(...)`, `Skip()`, `Is_Finished()`, `Close()`. `code/video/mfplayer.cpp` is
   the only file including `mfapi.h/mfreadwrite.h`; link `mfplat mfreadwrite mfuuid` (+
   `ole32`). Request `MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING` and output
   `MFVideoFormat_RGB32`; audio output PCM 16-bit 48000 stereo (the engine mix rate).
   Enable DXVA via `MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING`/D3D manager only if it is
   simple; software decode of 4K H.264 at 15 fps is fine on any modern CPU.
2. Audio: push decoded PCM into a new `VideoSinkClass : AudioStreamProducerClass`
   modelled line-for-line on `MovieSinkClass` (same group, ring depth, latency fold-in).
   The presentation clock is `Frames_Consumed()` converted to seconds; present the last
   decoded frame whose timestamp ≤ clock. If the movie has no audio track, use
   `Get_Game_Time_50()`.
3. Presentation: `Backend_Present_Video(void const* bgra, int pitch, int width, int
   height)` in `bgfxbackend.cpp` — a second texture (`BGRA8`, created/recreated on size
   change), linear sampler, quad into the drawable at aspect-fit (reuse the
   `Update_Scale_Info` maths on drawable size, not game size), clear to black, then
   `bgfx::frame()`. While a video plays, the game frame is not uploaded.
4. The skip overlay: `MovieSkip::Draw_Overlay` draws onto `VisibleSurface`. For the video
   path, render the overlay into a small `DSurface` region (its bounding rect) and upload
   that region as a third texture with 565→BGRA widening (reuse `_ConvertTable`) and a
   colour key on the overlay's background (black index → alpha 0), drawn after the video
   quad. If colour-keying is messy, draw a solid box behind the text; readability beats
   purity.
5. Lookup: in `Play_Movie(char const*)`, before `CCFileClass(name).Is_Available()`,
   derive the stem and try `<stem>.mp4` through `CCFileClass` (this walks user path →
   cwd → search folders, so `HD/GDI1.mp4` works). Replace the 20-byte buffers with a
   `std::string`/`char[64]`. The `MovieSkip::Playback` scope opens exactly where it does
   today.
6. Blocking loop: a `Movie_Play_Video(VideoPlayer&)` mirroring `VQAClass::Play_VQA`'s
   handling of focus loss, pause, `MovieSkip::Idle`, ESC, and the `nobreakout` parameter.
7. `Options.StretchMovies=no` for the video path means integer fit rather than aspect
   fit; keep the key meaningful.
8. `MSVQAnim` (menu animations) and `wdtsel.cpp` world-map clips stay VQA. Document.
9. Failure handling: if MF fails to open or decode, log once and fall back to the VQA in
   the same call.

## Tests

`tests/videoplayer`: a harness that links `mfplayer.cpp` and opens a tiny MP4 the test
generates with a Media Foundation sink writer at runtime (H.264 encoder is present on
Windows 10+; if the sink writer is unavailable in CI, generate the file lazily and skip
with a message). Checks: dimensions, frame count ≥ 1, first frame timestamp 0, audio
sample count > 0. No retail data.

## Deliverables

Code; Debug+Release; CTest; `manual/changes/native-video-playback.md`;
`manual/content/formats/video-files.md` (naming, container, codec expectations, where
files go, what stays VQA); update `manual/content/keys/stretchmovies.md`; play-test
checklist: any `GDI1.mp4` (even a re-encode of the VQA via ffmpeg) plays full-window at
4K, audio in sync at start and at 3 minutes, ESC skips, a missing/corrupt MP4 falls back
to VQA, briefing/win/lose/campaign-final movies all route through, co-op skip vote still
works in a LAN test if you can, and `Session.PlayMovies=no` still suppresses.
