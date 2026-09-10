# Movies and video

Commit `0281b88`; re-grep.

## 1. Inventory

Engine wrapper: `code/vqa.h/.cpp` (`VQAClass`, 1285 lines), `code/movie.h/.cpp`
(`Play_Movie` family), `code/movies.h/.cpp` (`VQHandle`, `Movie_Create/Play/Destroy`,
surface callbacks), `code/movieskip.h/.cpp` (ESC skip, network-synchronised vote,
overlay), `code/vqoption.cpp`, `code/unvqdec.cpp`, `code/unvqtblc.cpp` (15-bit → 565
LUT), `code/intro.cpp`, `code/msanim.cpp` (`MSVQAnim` drives a `VQHandle` for menu
animations), `code/vq.hh` (86-entry `VQType` enum, no strings).
Library: `code/vqalib/` (VQAPlay32, 22 files, static lib `VQALib`).
Audio: `code/audio/` — miniaudio device (`audiodevice_ma.cpp`), mixer, streams, and
`audiomovie.cpp` (345 lines), the VQA→audio bridge.

## 2. Request path

```cpp
void Play_Movie(char const * name, ThemeType theme=THEME_NONE, bool clrscrn_after=true,
                bool stretch=true, bool clrscrn_before=true);          // code/movie.h:24
void Play_Movie(VQType vq, ThemeType theme=THEME_NONE, bool clrscrn=true, bool stretch=true);
```
- `Play_Movie(VQType)` `code/movie.cpp:185-193`: `strcpy(_buf, Movies[vq]); strcat ".VQA"`
  into `static char _buf[20]`. Same in `Play_Ingame_Movie` `:220-228`. **Suffix and
  buffer size are the two hard-codes to generalise.**
- `Play_Movie(char const*)` `:78`: bails outside campaign unless `Session.PlayMovies`
  `:81`; opens `MovieSkip::Playback playback(name)` **before** the file test `:86` (peers
  count a locally-missing movie); `if (!CCFileClass(name).Is_Available()) return;` `:88`;
  `Movie_Create(name, HiddenSurface, Rect(), Rect(), 255, 1)` `:94`; rejects < 320×200
  `:98`; computes the aspect-fit `StretchRect` `:104-116` when `Options.StretchMovies`
  and `DSurface::AllowStretchBlits`.
- Bare-string sites: `code/init.cpp:430-439` (`EVA, WWLOGO, FS_TITLE, STARTUP`), `:1322`
  (`SIZZLE1`), `code/score.cpp:227`, `code/newmenu.cpp:111-119` (`TS_Title/FS_Title`),
  `code/intro.cpp:58-67` (`INTR%d`), `code/grphmenu.cpp:76`, `code/wdtsel.cpp:226`.
- `Movie_Create` `code/movies.cpp:111`: chooses `VQACCFileHandler` vs
  `VQAMixFileHandler` `:125-127`; draw callback `Movie_Blit_To_Screen` for fullscreen
  `:133-137`; `Open_And_Load_Buffers` `:145`; colour mode 4→1 `:154`;
  `Set_Primary_Color_Mode(565)` `:172-174`. `Movie_Play` `:213` is the blocking loop →
  `VQAClass::Play_VQA(last_frame, nobreakout)` `code/vqa.cpp:478` → `VQA_Play(...,
  VQAMODE_WALK, 0)` `:546` then `SurfaceDrawCallback()`. This loop is the idle/skip/
  focus/pause contract a new player must honour.
- Name table: `DynamicVectorClass<char const*> Movies` (`code/movie.h:20`,
  `code/movies.cpp:27`), filled by `RulesClass::Do_Movies` `code/rules.cpp:2071-2087` from
  `rules.ini [Movies]`; **order must match `code/vq.hh`**. `VQ_From_Name`
  `code/conquer.cpp:1347`. Scenario keys `Intro/Brief/Win/Lose/Action/PostScore/
  PreMapSelect` read `code/scenario.cpp:3802-3808`; play sites `:381-392, :416-428,
  :1198-1207, :1223-1227, :1241 (campaign FinalMovie), :1372-1379`.

## 3. Decoder assumptions

- VQA version 3 (`VQAHD_VER3`, `code/vqalib/vqafile.h:114-116`; branches at
  `loader.cpp:2751, 2958, 3066, 3169`). Colour mode 1/4 = 15-bit hicolor → decoded into
  native 565 via `Hicolor_Init_Table` + `UnVQ1_C1_TABLE/UnVQ2_4x4_Table`
  (`code/vqa.cpp:490-508`, `code/unvqtblc.cpp:45`). Codebooks translated in
  `Handle_Codebook_Event` `code/vqa.cpp:772`.
- 640×400 assumed in `code/_rect.cpp:17` (`VisibleRect` default), `MSVQAnim` ctor
  `code/msanim.cpp:593` (`InitialRect = ((w-640)/2, (h-400)/2, 640, 400)`),
  `code/mpscore.cpp:232`.
- Stage 1: decode into `HiddenSurface` at native size — `Movie_Lock_Surface`
  `code/movies.cpp:40` → `VQA_Set_DrawBuffer` `code/vqalib/task.cpp:1244` (byte offset,
  no scaling). Stage 2: `Movie_Blit_To_Screen` `code/movies.cpp:75` →
  `VisibleSurface->Blit_From(StretchRect, DrawSurface, InitialRect)` → `DSurface::Blit_From`
  `StretchBlt COLORONCOLOR` (`code/dsurface.cpp:509-511`) → `MovieSkip::Draw_Overlay` →
  `Video_Present_If_Dirty`. So a 4K frame today would be *downsampled by GDI into the game
  frame* and re-magnified by bgfx. Useless.
- The whole VQA file is `malloc`ed at once (`code/vqa.cpp:237`).

## 4. Audio bridge — reuse this

`code/audio/audiomovie.cpp`: `MovieSinkClass : AudioStreamProducerClass` `:33` — `Fill()`
`:36` retires consumed blocks and pulls more from the player; ring depth
`AUDIO_MOVIE_RING_BLOCKS = 4` (`audiodefs.hh:133`), group `AUDIO_GROUP_MOVIE`, mix rate
48000 (`audiodefs.hh:105`). Handlers wired in the `VQAClass` ctor `code/vqa.cpp:174-183`:
```cpp
intptr_t __cdecl Stream_Audio_Handler(VQAHandle*, long action, void* buffer, long nbytes); // :311
unsigned long __cdecl Timer_Callback_Audio_Handler(VQAHandle*);                             // :286
unsigned long __cdecl Simple_Timer_Callback_Audio_Handler(VQAHandle*);                      // :280
```
**Sync model: picture is slaved to the audio clock.** `Timer_Callback_Audio_Handler`
returns `_sink.Clock.Ticks(Stream->Frames_Consumed(), ...)` (60 Hz `VQA_TIMETICKS`,
`code/vqalib/vqaplay.h:107`) with device latency folded in `:212-213`; two blocks
pre-primed `:216-217`; falls back to `Get_Game_Time_50()` with audio off. A new decoder
pushes PCM through `AudioEngine.Acquire_Stream_Slot` / `AudioPushStreamProducerClass` the
same way and presents the frame whose timestamp ≤ audio time.

## 5. Skip and network

`MovieSkip::Playback` / `MovieSkip::Idle` (`code/movieskip.h:28-46`) — the ESC vote is
synchronised over the network; `Draw_Overlay` paints "press ESC" + counts onto
`VisibleSurface`. A new video path must call the same `Idle` hook each frame and keep the
`Playback` scope, or multiplayer campaigns (co-op) desync. The overlay is drawn on the 565
frame, so a direct-to-GPU video path must composite it separately (upload just the
overlay sub-rect as a small texture with a colour-key, or re-draw the text via a tiny
bgfx text path).

## 6. Presentation bottleneck

`Backend_Present` contract (`code/bgfxbackend.h:42-44`): "The pixels are 16 bit 565".
One texture at game resolution (`Backend_Set_Frame_Size` `code/bgfxbackend.cpp:402`,
`:421-423`). The BGRA8 fallback texture + `_ConvertBuffer` (`:486-494`) is the closest
existing 32-bit path. `Submit_Quad :167`, ortho projection, sampler flags, and the
aspect-fit maths in `Update_Scale_Info` are reusable. Window size ≠ frame size: in
fullscreen the window is the desktop (e.g. 3840×2160) even if `ScreenWidth` is smaller, so
a video quad drawn to the *drawable* size shows true 4K.

Needed: `Backend_Present_Video(pixels, format, width, height, dest rect)` — a second
`bgfx::TextureHandle` at the movie's resolution, BGRA8 (CPU YUV→RGB or decoder-provided
RGB), bilinear sampler, drawn to the drawable letterboxed by aspect. Custom YUV shaders
need `shaderc` (tools are OFF); CPU convert or hardware decode to RGB avoids that.

## 7. Codec options for a 32-bit MSVC /MT Windows build

No video library is vendored (`thirdparty/`: bgfx.cmake, miniaudio, lzo only). Options:

| Option | Build cost | Notes |
| --- | --- | --- |
| **Windows Media Foundation Source Reader** (`mfreadwrite.h`, `mfapi.h`, ships with Windows 10+) | none — link `mfplat.lib mfreadwrite.lib mfuuid.lib` | H.264/AAC in MP4 decodes everywhere; HEVC and AV1 need the free Store extensions; hardware decode via DXVA on the 9070 XT; outputs RGB32 or NV12 frames and PCM audio through one API. **Recommended for a personal Windows fork.** Not portable. |
| dav1d (AV1, BSD-2) | meson + NASM; `vcpkg install dav1d:x86-windows-static` is the easy route | Fast software decode, small files. IVF container is trivial to parse (32-byte header + 12-byte frame headers), so no demuxer needed. Audio needs a separate track (Opus via opusfile or WAV). Portable. |
| libvpx (VP9, BSD-3) | official MSVC support (`--target=x86-win32-vs17`, needs yasm/nasm) or vcpkg | Same IVF story. Slower than dav1d at 4K. |

Licence: project is GPLv3-or-later with EA §7 terms (`LICENSE.md:606-643`); BSD/MIT
codecs are fine; GPLv2-only is not; `--enable-nonfree` ffmpeg is never. New files carry
the "OpenTS contributors" header (`code/movies.cpp:1-8`), not the EA header.
Memory: 4K BGRA frame = 33 MB; 32-bit LAA gives ~4 GB on 64-bit Windows, adequate for
double-buffering plus decoder reference frames.

## 8. Seams summary

| Seam | Location |
| --- | --- |
| Request funnel | `Play_Movie(char const*)` `code/movie.cpp:78` |
| `.VQA` suffix + 20-byte buffer | `code/movie.cpp:190, :225` |
| Loose-file precedence (works today) | `code/ccfile.cpp:429` |
| Handle abstraction | `struct VQHandle` `code/movie.h:34-105` (`VQAClass *VQA` is the field to make polymorphic) |
| Blocking loop contract | `VQAClass::Play_VQA` `code/vqa.cpp:478` |
| Audio push + clock | `code/audio/audiomovie.cpp:33-296` |
| Skip/network vote | `code/movieskip.h:28-46` |
| Presentation | `Backend_Present` `code/bgfxbackend.cpp:473`; contract `bgfxbackend.h:42-44` |
| GDI downscale to bypass | `code/dsurface.cpp:509-511` |
