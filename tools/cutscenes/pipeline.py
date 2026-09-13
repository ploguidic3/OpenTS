"""Driver for the cutscene upscale pipeline.

``proof`` runs one movie end to end, ``compare`` renders the stills that decide
whether the look is worth the batch, ``batch`` runs the inventory and records
what happened in a manifest, and ``verify`` reports which movies still have no
output.
"""

from __future__ import annotations

import dataclasses
from pathlib import Path
import shutil
import sys
import time

import common
import dump_frames
import encode as encode_stage
import upscale as upscale_stage
from common import Config, PipelineError


def _sources_present(config: Config) -> list[str]:
    directory = config.work / "vqa"
    if not directory.is_dir():
        return []
    return sorted(p.stem.upper() for p in directory.iterdir()
                  if p.suffix.lower() == ".vqa")


def run_one(config: Config, name: str, source: Path | None = None,
            backend: str = "realesrgan", use_denoise: bool = True,
            fmt: str = "png", use_rife: bool = False, preview: bool = False,
            force: bool = False, keep_frames: bool = True) -> dict:
    """Runs stages 2 to 4 for one movie and returns its manifest entry."""
    name = name.upper()
    started = time.monotonic()
    common.log(f"[{name}]")

    media, _frames, audio = dump_frames.dump(config, name, source, force=force)
    dumped = time.monotonic()

    upscaled, ran, rate = upscale_stage.upscale(
        config, name, backend=backend, use_denoise=use_denoise, fmt=fmt, force=force)
    upscaled_at = time.monotonic()

    fps = media.fps or 15.0
    frames_for_encode = upscaled
    if use_rife:
        frames_for_encode = upscale_stage.interpolate(config, name, upscaled, force=force)
        fps *= 2

    output = encode_stage.encode(config, name, frames=frames_for_encode, fps=fps,
                                 audio=audio, preview=preview, force=force)
    finished = time.monotonic()

    entry = {
        "status": "ok",
        "source": {
            "codec": media.codec,
            "width": media.width,
            "height": media.height,
            "fps": media.fps,
            "frames": media.frames,
            "duration": round(media.duration, 3),
            "has_audio": media.has_audio,
        },
        "backend": backend,
        "denoise": config.denoise_filter if use_denoise else "",
        "rife": bool(use_rife),
        "preview": bool(preview),
        "output": str(output),
        "output_bytes": output.stat().st_size,
        "output_md5": common.md5_of(output),
        "frames_upscaled": ran,
        "upscale_frames_per_second": round(rate, 3),
        "seconds": {
            "dump": round(dumped - started, 2),
            "upscale": round(upscaled_at - dumped, 2),
            "encode": round(finished - upscaled_at, 2),
            "total": round(finished - started, 2),
        },
    }
    if not keep_frames:
        for directory in (config.frames_dir(name), config.denoised_dir(name),
                          config.upscaled_dir(name), config.interpolated_dir(name)):
            if directory.is_dir():
                shutil.rmtree(common.guard_output(directory))
        entry["frames_kept"] = False
    return entry


def command_proof(config: Config, args) -> int:
    manifest = common.load_manifest(config)
    entry = run_one(config, args.movie, source=args.source, backend=args.backend,
                    use_denoise=args.denoise, fmt=args.fmt, use_rife=args.rife,
                    preview=args.preview, force=args.force)
    common.record(manifest, args.movie, **entry)
    common.save_manifest(config, manifest)
    source = entry["source"]
    common.log("")
    common.log(f"Proof of {args.movie.upper()}")
    common.log(f"  source          {source['width']}x{source['height']} "
               f"@ {source['fps']:g} fps, {source['frames']} frames")
    common.log(f"  upscale         {entry['backend']} {config.upscale_model}"
               f"{' + ' + entry['denoise'] if entry['denoise'] else ''}, "
               f"{entry['upscale_frames_per_second']:g} frames/s")
    common.log(f"  output          {entry['output']}")
    common.log(f"  output size     {entry['output_bytes'] / (1 << 20):.2f} MiB")
    common.log(f"  total time      {entry['seconds']['total']:.1f}s")
    return 0


def command_compare(config: Config, args) -> int:
    """Renders one frame three ways so the look can be judged before the batch."""
    name = args.movie.upper()
    frames = common.frame_files(config.frames_dir(name))
    if not frames:
        raise PipelineError(f"no frames for {name}; run proof or dump_frames.py first")
    index = args.frame if args.frame is not None else len(frames) // 2
    if not 0 <= index < len(frames):
        raise PipelineError(f"frame {index} is outside 0..{len(frames) - 1}")
    original = frames[index]

    target = common.ensure_dir(config.compare_dir())
    plain = target / f"{name}-{index:05d}-original.png"
    shutil.copy2(original, common.guard_output(plain))

    upscaled = upscale_stage.upscale_frame(
        config, original, target / f"{name}-{index:05d}-x4.png", backend=args.backend)

    denoised_dir = upscale_stage.denoise(config, name)
    denoised = common.frame_files(denoised_dir)[index]
    denoised_up = upscale_stage.upscale_frame(
        config, denoised, target / f"{name}-{index:05d}-x4-denoise.png",
        backend=args.backend)

    common.log("")
    for path in (plain, upscaled, denoised_up):
        common.log(f"  {path}  ({path.stat().st_size / 1024:.0f} KiB)")
    return 0


def command_trial(config: Config, args) -> int:
    """Encodes one short segment several ways so the motion can be compared.

    Shimmer is temporal, so a still cannot show it. Each recipe is encoded to
    its own clip of the same frames, to be watched one against another.
    """
    name = args.movie.upper()
    frames = common.frame_files(config.frames_dir(name))
    if not frames:
        raise PipelineError(f"no frames for {name}; run dump_frames.py first")
    recipes = [r for r in config.trials
               if not args.only or r["name"] in args.only]
    if not recipes:
        raise PipelineError(
            "no recipe selected. config.json lists: "
            + ", ".join(r["name"] for r in config.trials)
        )

    length = args.length or config.trial_frames
    start = args.start if args.start is not None else max(0, len(frames) // 2 - length // 2)
    segment = frames[start:start + length]
    if not segment:
        raise PipelineError(f"frames {start} to {start + length} are outside {name}")

    fps = args.fps or common.frame_rate(config.frames_dir(name))
    root = common.ensure_dir(config.trial_dir(name))
    source = common.ensure_dir(root / "source")
    for stale in source.iterdir():
        stale.unlink()
    for position, frame in enumerate(segment, start=1):
        shutil.copy2(frame, source / f"{position:05d}{frame.suffix}")
    common.log(f"{name}: frames {start} to {start + len(segment) - 1}, "
               f"{len(recipes)} recipe(s)")

    produced = []
    for recipe in recipes:
        label = recipe["name"]
        common.log(f"[{label}]")
        settings = dataclasses.replace(
            config, upscale_model=recipe.get("model", config.upscale_model))
        stage = source
        if recipe.get("denoise"):
            common.log(f"  pre: {recipe['denoise']}")
            stage = upscale_stage.filter_frames(
                source, root / f"{label}-pre", recipe["denoise"],
                fps=fps, force=args.force)
        _target, _ran, rate = upscale_stage.upscale_dir(
            settings, stage, root / f"{label}-up",
            backend=args.backend, force=args.force)
        clip = encode_stage.encode(
            settings, name, frames=root / f"{label}-up", fps=fps,
            audio=None, force=True, post_filter=recipe.get("post", ""),
            target=root / f"{label}.mp4")
        produced.append((label, clip, rate))

    common.log("")
    common.log(f"Trial clips for {name}, {len(segment)} frames each:")
    for label, clip, rate in produced:
        common.log(f"  {label:<20} {clip}  ({clip.stat().st_size / (1 << 20):.1f} MiB"
                   + (f", {rate:.2f} frames/s)" if rate else ")"))
    return 0


def command_batch(config: Config, args) -> int:
    movies = [n.upper() for n in args.movies] or _sources_present(config)
    if not movies:
        raise PipelineError(
            f"no extracted movies in {config.work / 'vqa'}; run extract_movies.py first"
        )
    manifest = common.load_manifest(config)
    failures = []
    for position, name in enumerate(movies, start=1):
        common.log(f"--- {position}/{len(movies)} ---")
        if not args.force and config.output_file(name).is_file():
            common.log(f"[{name}] output already present")
            continue
        try:
            entry = run_one(config, name, backend=args.backend, use_denoise=args.denoise,
                            fmt=args.fmt, use_rife=args.rife, force=args.force,
                            keep_frames=not args.delete_frames)
        except PipelineError as error:
            entry = {"status": "failed", "error": str(error)}
            failures.append(name)
            common.log(f"[{name}] failed: {error}")
        common.record(manifest, name, **entry)
        common.save_manifest(config, manifest)
    common.log("")
    common.log(f"Batch finished: {len(movies) - len(failures)} ok, {len(failures)} failed")
    if failures:
        common.log("  failed: " + ", ".join(failures))
    return 1 if failures else 0


def command_verify(config: Config, args) -> int:
    """Reports what each named movie has: an output, a source, or neither."""
    movies = common.movie_names(args.rules, config.extra_movies)
    encoded, ready, optional, missing = [], [], [], []
    for name in movies:
        if config.output_file(name).is_file():
            encoded.append(name)
        elif config.vqa_file(name).is_file():
            ready.append(name)
        elif name.upper() in config.optional_movies:
            optional.append(name)
        else:
            missing.append(name)
    common.log(f"{len(movies)} named: {len(encoded)} encoded, {len(ready)} extracted "
               f"and not encoded, {len(optional)} optional, {len(missing)} with no source")
    common.log(f"  sources in {config.work / 'vqa'}")
    if ready:
        common.log("  ready:    " + ", ".join(ready))
    if optional:
        common.log("  optional: " + ", ".join(optional))
    if missing:
        common.log("  no source: " + ", ".join(missing))
    return 1 if missing else 0


def main() -> int:
    parser = common.base_parser(__doc__.splitlines()[0])
    subcommands = parser.add_subparsers(dest="command", required=True)

    def add_upscale_options(sub):
        sub.add_argument("--backend", choices=upscale_stage.BACKENDS, default="realesrgan",
                         help="realesrgan (default) or lanczos, which only rescales")
        sub.add_argument("--no-denoise", dest="denoise", action="store_false",
                         help="skip the configured denoise filter")
        sub.add_argument("--format", dest="fmt", choices=("png", "jpg"), default="png",
                         help="intermediate frame format")
        sub.add_argument("--rife", action="store_true",
                         help="double the frame rate after upscaling")
        sub.add_argument("--force", action="store_true",
                         help="redo work that is already present")

    proof = subcommands.add_parser("proof", help="run one movie end to end")
    proof.add_argument("movie")
    proof.add_argument("--source", type=Path, default=None,
                       help="decode this file instead of the extracted VQA")
    proof.add_argument("--preview", action="store_true",
                       help="encode only the first frames")
    add_upscale_options(proof)

    compare = subcommands.add_parser("compare", help="render one frame three ways")
    compare.add_argument("movie")
    compare.add_argument("--frame", type=int, default=None,
                         help="frame index (default: the middle frame)")
    compare.add_argument("--backend", choices=upscale_stage.BACKENDS,
                         default="realesrgan")

    trial = subcommands.add_parser(
        "trial", help="encode one short segment several ways for comparison")
    trial.add_argument("movie")
    trial.add_argument("--start", type=int, default=None,
                       help="first frame (default: centred on the movie)")
    trial.add_argument("--length", type=int, default=None,
                       help="frames per clip (default: trial_frames)")
    trial.add_argument("--only", nargs="*", default=None,
                       help="run only these recipes, by name")
    trial.add_argument("--fps", type=float, default=None,
                       help="frame rate (default: the rate the frames were dumped at)")
    trial.add_argument("--backend", choices=upscale_stage.BACKENDS,
                       default="realesrgan")
    trial.add_argument("--force", action="store_true",
                       help="redo work that is already present")

    batch = subcommands.add_parser("batch", help="run every extracted movie")
    batch.add_argument("movies", nargs="*",
                       help="movie names (default: everything extracted)")
    batch.add_argument("--delete-frames", action="store_true",
                       help="drop a movie's frames once it has been encoded")
    add_upscale_options(batch)

    verify = subcommands.add_parser("verify", help="report movies with no output")
    verify.add_argument("--rules", type=Path, default=None,
                        help="rules.ini to take the [Movies] inventory from")

    args = parser.parse_args()
    config = common.apply_common(args)
    return {
        "proof": command_proof,
        "compare": command_compare,
        "trial": command_trial,
        "batch": command_batch,
        "verify": command_verify,
    }[args.command](config, args)


if __name__ == "__main__":
    sys.exit(common.main_guard(main))
