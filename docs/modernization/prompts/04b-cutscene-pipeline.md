# Prompt 04b — Cutscene upscale pipeline (RX 9070 XT)

Context to load: `@docs/modernization/kb/03-movies-and-video.md`,
`@docs/modernization/kb/06-upscale-tooling-amd.md`.

Depends on 04a (the engine plays `<name>.mp4`).

## Goal

A repeatable, resumable pipeline in `tools/cutscenes/` that turns every campaign VQA
into a 3840×2400 H.264 MP4 the fork plays, using ffmpeg for decode/encode and
`realesrgan-ncnn-vulkan` on the Radeon for upscaling. Produce a one-clip proof first,
then the batch.

## Verify first

- `ffmpeg -decoders | findstr vqa` and `ffprobe` on one extracted VQA report
  `640x400`, 15 fps, `ws_vqa`. If ffprobe fails on TS's VQA3, say so and implement the
  `-DUMPVQA` fallback below before anything else.
- `realesrgan-ncnn-vulkan.exe -h` runs and lists the Radeon under `-g`.
- The `[Movies]` list in `rules.ini` and `code/vq.hh` give the full name inventory;
  `MOVIES01.MIX`/`MOVIES02.MIX` in `Run/` hold the files.

## Build

1. `extract_movies.py` — pull every `*.VQA` from the movie mixes into `work/vqa/` using
   the mix reader from 03b (or shell out to XCC Mixer with a documented command if that
   reader was skipped). Never write into `Run/`.
2. `dump_frames.py` — `ffmpeg -i X.VQA -vsync 0 work/frames/X/%05d.png` and
   `-vn -acodec pcm_s16le work/audio/X.wav`; verify the PNG count equals ffprobe's
   `nb_frames`. Optional engine fallback: add a `-DUMPVQA=<name>` developer switch to the
   fork that plays the VQA headless through `VQAClass` and writes each 565 frame as PNG
   via `writepcx.cpp`-style code; only if ffmpeg's decoder fails.
3. `upscale.py` — per movie: optional pre-denoise (`ffmpeg -vf hqdn3d=2:1:3:3` — make it
   a flag; evaluate on the proof clip), then
   `realesrgan-ncnn-vulkan -i work/frames/X -o work/up/X -n realesrgan-x4plus -s 4 -f png -g 0 -j 2:2:2`.
   Resumable: skip frames whose output exists. Log frames/s so I can estimate the batch.
   Optional `--rife` flag calling `rife-ncnn-vulkan` for 15→30 fps after upscaling.
4. `encode.py` —
   `ffmpeg -framerate <src fps> -i work/up/X/%05d.png -i work/audio/X.wav -vf scale=3840:2400:flags=lanczos -c:v libx264 -preset slow -crf 16 -pix_fmt yuv420p -c:a aac -b:a 192k -movflags +faststart out/X.mp4`.
   Also a `--preview` mode encoding only the first 300 frames.
5. `pipeline.py` — `proof <name>` runs 2–4 on one movie; `batch` runs all, with a
   manifest JSON recording per-movie status, timings and md5 of the output; `verify`
   checks every `[Movies]` name has an MP4 or is listed as intentionally skipped
   (`SIZZLE1`, logos, `TS_Title/FS_Title` menus are optional — make that a config list).
6. `README.md` with install steps for ffmpeg, the ncnn-vulkan zip, model files, Vulkan
   sanity check on the 9070 XT, disk-space estimate (~80k PNGs at 2560×1600 ≈ 400 GB
   uncompressed — offer `-f jpg` at quality 95 for the intermediate, or encode per movie
   and delete frames), and the ROCm/PyTorch alternative for trying stronger models on the
   proof clip.

## Proof clip

Run `pipeline.py proof GDI1` (or whichever is shortest — check durations first) end to
end and give me: frames/s achieved, output size, and three still comparisons (original,
x4, x4+denoise) as PNGs in `work/compare/`. Stop there; I will judge the look before the
batch.

## Deliverables

`tools/cutscenes/` with tests on a synthetic 16-frame VQA-shaped input (you can encode a
tiny MP4 and treat it as the source to exercise everything except VQA decode), the
README, the proof outputs, and a note in `manual/content/formats/video-files.md` pointing
at the tools.
