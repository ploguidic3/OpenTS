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

Download the Windows release zip from Xintao Wang's `Real-ESRGAN-ncnn-vulkan`
releases and unpack it anywhere. The zip carries `realesrgan-ncnn-vulkan.exe` and a
`models/` folder holding `realesrgan-x4plus.param` and `realesrgan-x4plus.bin`; the
executable finds the models relative to itself, so keep them together. Put the folder
on `PATH` or set `OPENTS_REALESRGAN`.

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
movie whose MP4 is already in `out/` is skipped, and `--force` overrides both.

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

### Denoise

`--denoise` runs `hqdn3d=2:1:3:3` over the frames before the upscale. Real-ESRGAN
works frame by frame, so film grain resolves differently in consecutive frames and
shimmers. Judge it on one clip with `pipeline.py compare` before spending the batch
on it; the filter costs detail as well as grain.

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

## Archives this reader will not open

`mixreader.py` reads a plain or extended MIX index. An archive whose index is
encrypted is refused by name: the engine decrypts those with a key that is not in
this repository. Unpack such an archive with XCC Mixer instead and put the `.VQA`
files into `work/vqa/` by hand; the rest of the pipeline reads them from there.
