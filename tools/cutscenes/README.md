# Cutscene upscale pipeline

Turns the retail Tiberian Sun and Firestorm VQAs into 3840x2400 H.264 MP4s that the
fork plays in place of them. ffmpeg decodes and encodes; `realesrgan-ncnn-vulkan`
enlarges the frames on the Radeon. [Video files](../../manual/content/formats/video-files.md)
describes what the engine does with the result.

Nothing here writes into `Run/`. A destination inside it is refused.

## Install

### ffmpeg

Version 5.1 or later. Earlier builds mis-handle VQA version 3, which is what Tiberian
Sun ships. A gyan.dev or BtbN Windows build carries `ffmpeg.exe` and `ffprobe.exe`;
put both on `PATH`, or set `OPENTS_FFMPEG` and `OPENTS_FFPROBE` to their full paths.

Check the decoder is there:

```powershell
ffmpeg -decoders | findstr vqa       # vqavideo   Westwood Studios VQA (codec ws_vqa)
ffmpeg -demuxers  | findstr wsvqa    # wsvqa      Westwood Studios VQA
```

`ffprobe` is optional. Without it the same numbers are read back from an ffmpeg
null-decode, at the cost of one extra decode pass per movie.

### realesrgan-ncnn-vulkan

Take the zip from the **`xinntao/Real-ESRGAN`** releases, not from the
`Real-ESRGAN-ncnn-vulkan` repository of the same author. The binary is built in the
latter and published in the former, and only the published one carries the trained
models: `realesrgan-ncnn-vulkan-20220424-windows.zip` under tag `v0.2.5.0` is about
45 MB and holds `realesrgan-ncnn-vulkan.exe`, `vcomp140.dll`, `vcomp140d.dll` and a
`models/` folder. The zip attached to the build repository's own `v0.2.0` release is
about 2 MB and holds the same executable with no models at all; the executable then
fails to open the model as a process fault.

Unpack it anywhere and put the folder on `PATH`, or set `OPENTS_REALESRGAN` to the
executable. The models are looked for in `models/` beside it; name another folder in
`model_path` in `config.json` or in `OPENTS_REALESRGAN_MODELS`.

The 2022 zip carries `realesrgan-x4plus`, `realesrgan-x4plus-anime` and
`realesr-animevideov3`, and the pipeline's default is `realesrgan-x4plus`. The
animevideov3 files carry the scale in their names, so the model named
`realesr-animevideov3` is `realesr-animevideov3-x4.param` on disk at `-s 4`.

```powershell
dir "<unpack folder>\models\*.param"     # the model names available
```

`rife-ncnn-vulkan` is the same shape and only needed for `--rife`. Set `OPENTS_RIFE`.

### Vulkan on the RX 9070 XT

The Radeon driver supplies the Vulkan runtime; nothing else is needed. Confirm the
card is the device the tool will pick:

```powershell
vulkaninfo --summary                 # from the LunarG SDK, if installed
realesrgan-ncnn-vulkan.exe -h        # lists the GPUs available to -g
```

`-g 0` in `config.json` selects the first Vulkan device. On a machine with an
integrated GPU as well, check which index the Radeon holds and set `gpu` to it.

### Python

Python 3.11 or later. The scripts use the standard library only.

## Run

```powershell
python extract_movies.py --data ..\..\Run       # MOVIES01/02.MIX -> work\vqa\
python pipeline.py proof GDI1                   # one movie, end to end
python pipeline.py compare GDI1                 # three stills to judge the look
python pipeline.py batch                        # everything extracted
python pipeline.py verify --rules ..\..\Run\rules.ini
```

Every stage is resumable. A frame that already has an output is not run again, a
movie whose MP4 is already in `out/` is skipped, and `--force` overrides both. The
upscale writes each frame straight into `work/up/<NAME>`, so the count of files there
is the live progress and an interrupted run resumes from it. A frame left half written
is detected and redone rather than counted as finished.

The upscale prints a line every few seconds once it is under way:

```
  upscale: 497 frame(s) through realesrgan -> work\up\GDI_M02
    64/497 frames, 4.71 frames/s, about 1.5 min left
```

Under Vulkan the GPU load swings between full and idle rather than sitting at 100%:
each frame is a short burst of inference between reading the PNG in and writing the
larger one out. A card with a zero-RPM idle mode may never spin its fans up.

`pipeline.py batch` writes `work/manifest.json` after every movie, recording each
one's source geometry, the stage timings, the frames-per-second the upscale achieved,
and the size and MD5 of the output. A movie that fails is recorded as failed and the
batch continues.

The stage scripts run on their own as well, which is how to redo one stage of one
movie: `dump_frames.py`, `upscale.py`, `encode.py`. `--verbose` echoes every external
command line.

### Settings

`config.json` holds the delivery size, the model and GPU index, the encoder settings,
the denoise filter, and `optional_movies` — the names `verify` accepts as having no
MP4. The logos, the two title screens and `SIZZLE1` are in that list.

### Sound

The sound track is rewritten to 48 kHz stereo rather than kept at the VQA's own
22050 Hz mono. The Windows decoder the engine plays these files with mis-handles that
rate, and the symptom is a stuttering picture with broken sound rather than an error.

### Shimmer

Real-ESRGAN sees one frame at a time. It has no idea what the previous frame looked
like, so anything in the picture that is noise rather than content is reconstructed
differently every frame, and the result crawls. A VQA gives it plenty to work with: the
codec quantises 4x4 blocks against a per-frame codebook at 15-bit colour, so block
edges, dither and codebook churn all move frame to frame, and the upscale sharpens
every bit of it into detail that was never there.

Two things help, in this order:

- **Clean the source first.** `deblock=filter=strong:block=4` matches the codec's block
  size, and a temporal denoise such as `hqdn3d=4:3:6:6` removes what varies between
  frames while keeping what does not. This is where most of the shimmer goes.
- **Change the model.** `realesr-animevideov3` is trained for video and is markedly
  steadier than `realesrgan-x4plus`, at the cost of flattening fine texture. Which
  trade is right depends on the shot.

A still cannot show shimmer, so `compare` cannot settle this. `trial` encodes the same
short segment several ways and leaves the clips side by side to be watched:

```powershell
python pipeline.py trial GDI_M02                       # every recipe, 120 frames
python pipeline.py trial GDI_M02 --only plain deblock  # just these two
python pipeline.py trial GDI_M02 --start 900 --length 200
```

The recipes are the `trials` list in `config.json`: a name, a model, a filter chain to
run before the upscale, and one to run at encode time. Add your own rather than editing
the shipped ones. The clips land in `work/trial/<NAME>/` at the delivery resolution,
which is what you will be watching them at.

Once a recipe wins, put its parts into `denoise_filter`, `upscale_model` and
`post_filter` and run the batch.

### Denoise

`--denoise` runs the `denoise_filter` from `config.json` over the frames before the
upscale. `--post-filter` on `encode.py` appends a filter chain after the scale, which
is where a temporal smoother such as `atadenoise` goes. Both cost real detail as well
as noise.

### Frame rate

The encode keeps the source rate, which is 15 fps. `--rife` doubles it with
`rife-ncnn-vulkan` after the upscale, and the engine plays whatever rate the file
carries.

## Disk

A 640x400 frame upscaled four times is 2560x1600. Across roughly 90 minutes of
cutscenes, about 80,000 frames, the PNG intermediates come to several hundred
gigabytes. Three ways to keep that down, in the order worth trying:

- `--delete-frames` on `batch` drops a movie's frames as soon as its MP4 is written,
  so the peak is one movie rather than all of them. A long movie is about 2,700
  frames, roughly 15 GB of PNG.
- `--format jpg` writes the upscale's output as JPEG instead of PNG. It is quantised
  a second time before x264 quantises it again; prefer `--delete-frames`.
- Run `batch` over a subset of names and work through the inventory in passes.

The delivered MP4s are small by comparison: at CRF 16 a 3-minute cutscene lands around
150-250 MB.

## Stronger models

The ncnn build runs the models in its own `models/` folder and nothing else. To try
HAT, SPAN, RealPLKSR or DAT on the proof clip, install PyTorch with ROCm following
AMD's "Use ROCm on Radeon" guide for Windows — the RX 9070 XT is gfx1201 and is on
that matrix — then `pip install spandrel` and drive the model from a script that reads
`work/frames/<NAME>` and writes `work/up/<NAME>` under the same names. `encode.py`
does not care which tool produced the frames.

chaiNNer is the quicker way to compare models by eye before scripting one.

## Tests

```powershell
python -m unittest discover -s tests
```

The tests synthesise everything they read: MIX archives are written by the test, and
the movie stages run on a generated 16-frame clip standing in for a VQA. No retail
asset is touched. The stage tests need ffmpeg and are skipped without it; they use the
`lanczos` upscale backend, which rescales with ffmpeg instead of a network. That
backend exists to exercise the pipeline without a GPU and is not an upscale.

## What has not been run

The pipeline was written and tested on Linux without a Radeon, a retail install, or
ffprobe. Three things are therefore unproven and are worth treating as the first
run rather than a regression when they misbehave:

- No VQA has been probed or decoded. That ffmpeg reads Tiberian Sun's version 3 files
  is taken from its source, not observed here, and is what `proof` establishes first.
- `realesrgan-ncnn-vulkan` has never been invoked. Its arguments follow its
  documentation. Only the `lanczos` backend has run.
- No MIX archive from the retail install has been opened. `mixreader.py` was exercised
  against archives the tests build to the same layout.

Run `pipeline.py proof` on one movie before a batch, which is what it is for.

## When the upscaler dies without a message

`realesrgan-ncnn-vulkan` treats a model it cannot open as a process fault: it prints a
`_wfopen ... failed` line and exits with a Windows status code such as `0xC0000409`
rather than an error. The pipeline checks the model pair before launching it and
reports what is missing, so that crash should not reach you; if it does, the last line
the tool printed names the file it wanted.

## Archives this reader will not open

`mixreader.py` reads a plain or extended MIX index. An archive whose index is
encrypted is refused by name: the engine decrypts those with a key that is not in
this repository. Unpack such an archive with XCC Mixer instead and put the `.VQA`
files into `work/vqa/` by hand; the rest of the pipeline reads them from there.
