"""Stage 3: enlarge the dumped frames, optionally denoising first.

Real-ESRGAN reads a whole directory in one run because loading the model per
frame costs more than the upscale does. To stay resumable the frames that have
no output yet are linked into a staging directory and only those are run, so an
interrupted movie continues rather than starting again.
"""

from __future__ import annotations

import os
from pathlib import Path
import shutil
import sys
import tempfile
import time

import common
from common import Config, PipelineError


BACKENDS = ("realesrgan", "lanczos")


def denoise(config: Config, name: str, force: bool = False) -> Path:
    """Runs the configured denoise filter over the dumped frames.

    Film grain makes a frame-by-frame upscale shimmer, so the filter runs before
    the upscale rather than after it.
    """
    name = name.upper()
    source = config.frames_dir(name)
    target = config.denoised_dir(name)
    frames = common.frame_files(source)
    if not frames:
        raise PipelineError(f"no frames in {source}; run dump_frames.py first")
    if force and target.is_dir():
        shutil.rmtree(common.guard_output(target))
    if len(common.frame_files(target)) == len(frames):
        common.log(f"  denoise: {len(frames)} frames already present")
        return target
    common.ensure_dir(target)
    common.run_ffmpeg([
        "-framerate", "15", "-i", common.frame_pattern(source),
        "-vf", config.denoise_filter,
        "-fps_mode", "passthrough", str(target / "%05d.png"),
    ])
    common.log(f"  denoise: {config.denoise_filter} -> {target}")
    return target


def _stage_missing(source: Path, target: Path, suffix: str, staging: Path) -> list[Path]:
    """Links the frames with no output yet into a staging directory."""
    pending = []
    for frame in common.frame_files(source):
        if (target / f"{frame.stem}{suffix}").is_file():
            continue
        link = staging / frame.name
        try:
            link.hardlink_to(frame)
        except (OSError, NotImplementedError):
            shutil.copy2(frame, link)
        pending.append(frame)
    return pending


def model_directory(config: Config) -> Path:
    """Returns the folder holding the .param and .bin model files."""
    if config.model_path:
        return Path(config.model_path)
    override = os.environ.get("OPENTS_REALESRGAN_MODELS")
    if override:
        return Path(override)
    return common.realesrgan().resolve().parent / "models"


def check_model(config: Config) -> Path:
    """Reports a model the upscaler could not load before it is launched.

    realesrgan-ncnn-vulkan fails a missing model as a process fault rather than
    an error, so the pair of files is checked here and the models actually
    present are named.
    """
    directory = model_directory(config)
    advice = (
        "Set model_path in config.json, or OPENTS_REALESRGAN_MODELS, to the folder "
        "holding the model files, and upscale_model to one of the models in it."
    )
    if not directory.is_dir():
        raise PipelineError(f"{directory} is not a directory. {advice}")
    missing = [
        path.name for path in (
            directory / f"{config.upscale_model}.param",
            directory / f"{config.upscale_model}.bin",
        ) if not path.is_file()
    ]
    if missing:
        available = sorted({path.stem for path in directory.glob("*.param")})
        found = (f"Models in that folder: {', '.join(available)}."
                 if available else "That folder holds no .param file at all.")
        raise PipelineError(
            f"{directory} has no {' or '.join(missing)} for model "
            f"{config.upscale_model}. {found} {advice}"
        )
    return directory


def _run_realesrgan(config: Config, staging: Path, staged_out: Path, fmt: str) -> None:
    common.run([
        common.realesrgan(),
        "-i", staging, "-o", staged_out,
        "-m", check_model(config),
        "-n", config.upscale_model,
        "-s", str(config.upscale_factor),
        "-f", fmt,
        "-g", config.gpu,
        "-j", config.threads,
    ], capture=True)


def _run_lanczos(config: Config, staging: Path, staged_out: Path, fmt: str) -> None:
    """Scales with ffmpeg instead of a network. Not an upscale; for dry runs only."""
    factor = config.upscale_factor
    for frame in common.frame_files(staging):
        common.run_ffmpeg([
            "-i", frame,
            "-vf", f"scale=iw*{factor}:ih*{factor}:flags=lanczos",
            str(staged_out / f"{frame.stem}.{fmt}"),
        ])


def run_backend(config: Config, staging: Path, staged_out: Path,
                backend: str, fmt: str) -> None:
    """Runs one upscale backend over a directory of frames."""
    if backend == "realesrgan":
        _run_realesrgan(config, staging, staged_out, fmt)
    elif backend == "lanczos":
        _run_lanczos(config, staging, staged_out, fmt)
    else:
        raise PipelineError(f"unknown upscale backend {backend}")


def upscale_frame(config: Config, frame: Path, target: Path,
                  backend: str = "realesrgan", fmt: str = "png") -> Path:
    """Upscales a single frame to an explicit destination file."""
    with tempfile.TemporaryDirectory(prefix="upscale-one-") as temp:
        staging = Path(temp) / "in"
        staged_out = Path(temp) / "out"
        staging.mkdir()
        staged_out.mkdir()
        shutil.copy2(frame, staging / frame.name)
        run_backend(config, staging, staged_out, backend, fmt)
        produced = sorted(staged_out.iterdir())
        if not produced:
            raise PipelineError(f"{backend} produced nothing for {frame}")
        common.ensure_dir(target.parent)
        shutil.move(str(produced[0]), str(common.guard_output(target)))
    return target


def upscale(config: Config, name: str, backend: str = "realesrgan",
            use_denoise: bool = False, fmt: str = "png",
            force: bool = False) -> tuple[Path, int, float]:
    """Enlarges one movie's frames. Returns the directory, frames run and frames/s."""
    name = name.upper()
    source = denoise(config, name, force=force) if use_denoise else config.frames_dir(name)
    if not common.frame_files(source):
        raise PipelineError(f"no frames in {source}; run dump_frames.py first")
    target = config.upscaled_dir(name)
    if force and target.is_dir():
        shutil.rmtree(common.guard_output(target))
    common.ensure_dir(target)

    suffix = f".{fmt}"
    with tempfile.TemporaryDirectory(prefix=f"upscale-{name}-") as temp:
        staging = Path(temp) / "in"
        staged_out = Path(temp) / "out"
        staging.mkdir()
        staged_out.mkdir()
        pending = _stage_missing(source, target, suffix, staging)
        if not pending:
            total = len(common.frame_files(target))
            common.log(f"  upscale: {total} frames already present")
            return target, 0, 0.0
        common.log(f"  upscale: {len(pending)} frame(s) through {backend}")
        started = time.monotonic()
        run_backend(config, staging, staged_out, backend, fmt)
        elapsed = max(time.monotonic() - started, 1e-6)
        produced = sorted(staged_out.iterdir())
        if len(produced) != len(pending):
            raise PipelineError(
                f"{name}: {backend} produced {len(produced)} of {len(pending)} frames"
            )
        for frame in produced:
            shutil.move(str(frame), str(target / f"{frame.stem}{suffix}"))

    rate = len(pending) / elapsed
    common.log(f"  upscale: {len(pending)} frames in {elapsed:.1f}s ({rate:.2f} frames/s)")
    return target, len(pending), rate


def interpolate(config: Config, name: str, source: Path, force: bool = False) -> Path:
    """Doubles the frame rate with RIFE. Renumbers the result to %05d."""
    name = name.upper()
    target = config.interpolated_dir(name)
    if force and target.is_dir():
        shutil.rmtree(common.guard_output(target))
    expected = len(common.frame_files(source)) * 2
    if len(common.frame_files(target)) == expected and expected:
        common.log(f"  rife: {expected} frames already present")
        return target
    common.ensure_dir(target)
    with tempfile.TemporaryDirectory(prefix=f"rife-{name}-") as temp:
        staged_out = Path(temp)
        common.run([common.rife(), "-i", source, "-o", staged_out, "-g", config.gpu],
                   capture=True)
        for index, frame in enumerate(sorted(staged_out.iterdir()), start=1):
            shutil.move(str(frame), str(target / f"{index:05d}{frame.suffix}"))
    common.log(f"  rife: {len(common.frame_files(target))} frames -> {target}")
    return target


def main() -> int:
    parser = common.base_parser(__doc__.splitlines()[0])
    parser.add_argument("movie", help="movie name, without an extension")
    parser.add_argument("--backend", choices=BACKENDS, default="realesrgan",
                        help="realesrgan (default) or lanczos, which only rescales")
    parser.add_argument("--denoise", action="store_true",
                        help="run the configured denoise filter before upscaling")
    parser.add_argument("--format", dest="fmt", choices=("png", "jpg"), default="png",
                        help="intermediate frame format; jpg trades quality for disk")
    parser.add_argument("--rife", action="store_true",
                        help="double the frame rate with rife-ncnn-vulkan afterwards")
    parser.add_argument("--force", action="store_true",
                        help="discard frames already upscaled")
    args = parser.parse_args()
    config = common.apply_common(args)

    common.log(f"Upscaling {args.movie.upper()}")
    target, _count, _rate = upscale(config, args.movie, backend=args.backend,
                                    use_denoise=args.denoise, fmt=args.fmt,
                                    force=args.force)
    if args.rife:
        interpolate(config, args.movie, target, force=args.force)
    return 0


if __name__ == "__main__":
    sys.exit(common.main_guard(main))
