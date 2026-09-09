# Upscale tooling for an AMD Radeon RX 9070 XT

Everything here runs on Windows without CUDA. Two runtimes cover the GPU:

| Runtime | Use for | Status (Sep 2026) |
| --- | --- | --- |
| **Vulkan via ncnn** (`realesrgan-ncnn-vulkan`, `rife-ncnn-vulkan`, Video2X, chaiNNer's NCNN backend) | Batch image and frame upscaling; zero driver stack beyond the Radeon driver | Works on any Vulkan GPU; the simplest path. Older binaries (2022) still run; RDNA4 is a standard Vulkan device. |
| **PyTorch + ROCm on Windows** (ROCm 7.2.1 components, PyTorch 2.9, Python 3.12) | Newer models via chaiNNer/spandrel (HAT, SPAN, RealPLKSR, DAT), custom scripts | RX 9070 XT (gfx1201) is on AMD's Windows support matrix. "Entire ROCm stack not yet supported on Windows" — PyTorch is, which is all we need. FP16 fine. |

Start with ncnn/Vulkan. Move to ROCm only if a specific model is worth it.

## A. Cutscenes: VQA → 4K video

### A1. Extract frames and audio

ffmpeg's `wsvqa` demuxer and `vqavideo` decoder handle Tiberian Sun's version-3
hi-colour VQAs (the installed 6.1 build carries the "VQA3 shouldn't have a color palette"
path; use ffmpeg ≥ 5.1). Movies live in `MOVIES01.MIX`/`MOVIES02.MIX`; extract them with
XCC Mixer or the fork's own mix reader (prompt 04b writes one). Then:

```powershell
ffprobe GDI1.VQA                                   # expect 640x400, 15 fps, ws_vqa, ws_snd1/pcm
ffmpeg -i GDI1.VQA -vsync 0 frames\GDI1_%05d.png   # -vsync 0 keeps the native frame count
ffmpeg -i GDI1.VQA -vn -acodec pcm_s16le GDI1.wav
```
If a specific file fails to decode (ffmpeg's VQA3 path is less exercised than the
engine's), the fallback is a `-DUMPVQA=<name>` developer switch in the fork that writes
PNGs from `VQAClass`'s own decoder — prompt 04b includes it as an option.

### A2. Upscale

`realesrgan-ncnn-vulkan -i frames -o up -n realesrgan-x4plus -s 4 -f png -g 0`
- `realesrgan-x4plus` for live-action/CGI footage; `realesr-animevideov3` is wrong for TS.
- `-g 0` selects the first Vulkan GPU; `-j 2:2:2` tunes load/proc/save threads.
- 640×400 ×4 = 2560×1600. That is the honest ceiling of a single x4 pass; a second x4
  pass adds nothing real. Let ffmpeg lanczos it to the delivery size.
- Frame-by-frame ESRGAN flickers slightly on film grain. Mitigations: `hqdn3d` or
  `nlmeans` denoise *before* upscaling, and a light `tmix=frames=2` after. Try both on one
  clip before committing hours.
- Optional: `rife-ncnn-vulkan` 15 → 30 fps. Do it after upscaling, on the PNGs, and only
  if you like the look; the engine path plays whatever frame rate the file carries.
- Budget: ~90 minutes of cutscenes across TS+FS ≈ 80k frames at 640×400. Expect roughly
  5–10 frames/s for x4 on a 9070 XT under Vulkan, so a few hours total. Script it per
  movie so a failure does not lose the batch.

### A3. Encode

Target 3840×2400 (16:10 like the source; the engine letterboxes to 16:9). For the Media
Foundation engine path (prompt 04a, recommended for the fork):
```powershell
ffmpeg -framerate 15 -i up\GDI1_%05d.png -i GDI1.wav -vf "scale=3840:2400:flags=lanczos" ^
  -c:v libx264 -preset slow -crf 16 -pix_fmt yuv420p -c:a aac -b:a 192k -movflags +faststart GDI1.mp4
```
For the dav1d/IVF path: `-c:v libsvtav1 -crf 24 -preset 6` to `.ivf` (video only) plus
`.opus`/`.wav` for audio. Keep the original 15 fps unless you interpolated.

### A4. Where the files go

Same base name as the VQA, next to the game or in the `HD` folder from prompt 02:
`GDI1.mp4` beside/instead of `GDI1.VQA`. The engine tries the new extension first.
Menu-animation VQAs driven by `MSVQAnim` (`code/msanim.cpp`) are a separate, later job.

## B. Sprites: SHP → 2× SHP on the same palette

Palettised art is the awkward case. Pipeline per SHP (prompt 05c writes the tool):

1. **Decode** the SHP to per-frame PNGs *plus* two masks: alpha (index 0) and remap
   (indices 16–31 in `UNIT*.PAL`). Shadow frames (second half) are 1-bit; export them as
   masks, not colour. Keep each frame's `X, Y` offsets and the SHP logical `Width/Height`.
2. **Upscale** the RGB frame 2× (`realesrgan-x4plus` then downsample 2×, or
   `realesr-general-x4v3 -s 2`). Upscale the masks with nearest or a thresholded
   bilinear — never ESRGAN — so remap and transparency edges stay crisp.
3. **Re-quantise** to the target palette: for non-remap pixels, nearest colour among
   indices 32–255 (plus theater-specific ranges); for remap-mask pixels, nearest among
   16–31 by luminance. Optional error diffusion at very low amplitude; ordered dithering
   looks wrong at 2×. `Pillow` `Image.quantize(palette=…, dither=0)` is not enough because
   of the remap constraint — write the nearest-index search directly (numpy, 256-entry
   LUT in Lab space).
4. **Encode** a 2× SHP: doubled header `Width/Height`, doubled per-frame `X, Y, Width,
   Height`, same `Count`, same frame order, RLE where the source used it, `Color[3]`
   copied. Shadow frames from the upscaled 1-bit masks.
5. **Validate** by decoding the result again and diffing frame counts, offsets ×2, and
   palette range membership; render one frame through the fork's blitter in a harness.

Voxels need no art work: prompt 05a renders them at 2× in the engine. Terrain TMP tiles
are last (prompt 05d): the Z-data and extra-image planes must be upscaled with nearest
and the 96×48 diamond mask regenerated.

## C. Tool inventory

- ffmpeg/ffprobe ≥ 5.1 (VQA3), libx264, libsvtav1.
- `realesrgan-ncnn-vulkan` (Xintao's release zip) — models `realesrgan-x4plus`,
  `realesr-general-x4v3`; `rife-ncnn-vulkan` optional.
- chaiNNer (GUI, NCNN or PyTorch-ROCm backend) for experimenting with models before
  scripting.
- Python 3.12 with `numpy`, `Pillow`, `scikit-image` (Lab conversion) for the SHP tool.
- XCC Mixer or the fork's mix reader to pull assets out of `.MIX`; the fork's own
  `ShapeSet`/`IsoTileSet` layout is the authoritative format reference
  (`code/shapeset.h`, `code/isotype.h`).
- PyTorch ROCm (optional): install per AMD's "Use ROCm on Radeon" Windows guide, then
  `pip install spandrel` and drive models from a script; keep it behind the same
  frames-in/frames-out contract as the ncnn path so the pipeline does not care which ran.

Sources: AMD Windows ROCm compatibility matrix (RX 9070 XT / gfx1201, ROCm 7.2.1,
PyTorch 2.9); Real-ESRGAN ncnn-vulkan releases; FFmpeg `libavcodec/vqavideo.c`.
