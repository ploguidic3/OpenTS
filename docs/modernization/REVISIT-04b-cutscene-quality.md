# Revisit: cutscene upscale quality

The `04b` batch was encoded with `realesr-animevideov3` over deblocked and denoised
frames. That combination was chosen by watching `pipeline.py trial` clips, not from a
still, because the defect that decides this is temporal. It is good enough to play;
it is not the best the source can give.

## What the choice actually was

Real-ESRGAN sees one frame at a time. A VQA gives it a lot of frame-varying noise to
mistake for detail: the codec quantises 4x4 blocks against a per-frame codebook at
15-bit colour, so block edges, dither and codebook churn all move between frames, and a
per-frame upscale reconstructs them differently each time. The result crawls.

Two things were tried against it, in `config.json` under `trials`:

- Cleaning the source first. `deblock=filter=strong:block=4` matched to the codec's
  block size, then `hqdn3d=4:3:6:6` for what varies between frames. Most of the shimmer
  went here.
- Changing the model. `realesr-animevideov3` is trained for video and is markedly
  steadier than `realesrgan-x4plus`, at the cost of flattening fine texture.

`realesrgan-x4plus` alone shimmered badly enough to be distracting. The
`kb/06-upscale-tooling-amd.md` claim that animevideov3 is "wrong for TS" was written
about fidelity on live action and did not anticipate how much the temporal instability
would matter; that file has been corrected.

## What to try next

The honest ceiling of a per-frame model has been reached. The next real gain needs a
model that propagates information across frames rather than a sharper one applied to
each: BasicVSR++ is the established open-weight choice, and SeedVR2 and FlashVSR are the
2026 successors. None of them runs under ncnn; on the RX 9070 XT (gfx1201) that means
PyTorch with ROCm on Windows, per AMD's "Use ROCm on Radeon" guide, driving the model
from a script.

Nothing else in the pipeline has to change. `upscale.py` is frames in and frames out, so
a new upscaler that reads `work/frames/<NAME>` and writes `work/up/<NAME>` under the same
names drops in, and `encode.py` does not care what produced the frames. Adding it as a
third `BACKENDS` entry beside `realesrgan` and `lanczos` is the shape to aim for.

Judge any candidate with `pipeline.py trial` on the same segment before committing to a
batch. A still cannot show the defect being fixed.

## Smaller things left undone

- `VEGAWIN` and `MEKATAK2` have no source. The names came from the enumerators in
  `code/vq.hh` rather than from `rules.ini [Movies]`, and a MIX can only be asked for
  exact names, so the files may exist under names that were never asked for. Extract
  `rules.ini` from `TIBSUN.MIX` and pass `--rules` to settle it.
- The inventory is `rules.ini [Movies]` plus `config.json`'s `extra_movies`. Anything
  else the code asks for by a bare name is not encoded and plays as a VQA. `SIZZLE1` and
  `INTR0` through `INTR3` are listed; the other bare-string sites in
  `kb/03-movies-and-video.md` section 2 have not been checked against it.
- `--rife` for 15 to 30 fps was never evaluated.
- `post_filter` is empty. A temporal smoother such as `atadenoise` after the scale was
  written and tested but not judged against a clip.
