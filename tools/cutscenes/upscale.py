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
import threading
import time

import common
from common import Config, PipelineError


BACKENDS = ("realesrgan", "lanczos")


def filter_frames(source: Path, target: Path, filter_string: str,
                  fps: float = 15.0, force: bool = False) -> Path:
    """Runs an ffmpeg filter chain over a frame sequence into another directory.

    The sequence is passed as one input so temporal filters see the frames
    either side of each one.
    """
    frames = common.frame_files(source)
    if not frames:
        raise PipelineError(f"no frames in {source}")
    if force and target.is_dir():
        shutil.rmtree(common.guard_output(target))
    if len(common.frame_files(target)) == len(frames):
        return target
    common.ensure_dir(target)
    common.run_ffmpeg([
        "-framerate", f"{fps:g}", "-i", common.frame_pattern(source),
        "-vf", filter_string,
        "-fps_mode", "passthrough", str(target / "%05d.png"),
    ])
    return target


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
    filter_frames(source, target, config.denoise_filter, force=force)
    common.log(f"  denoise: {config.denoise_filter} -> {target}")
    return target


# Trailing bytes of a complete file: a PNG ends with IEND and its CRC, a JPEG
# with the end-of-image marker.
_TERMINATORS = {".png": b"IEND\xae\x42\x60\x82", ".jpg": b"\xff\xd9", ".jpeg": b"\xff\xd9"}


def is_complete(path: Path) -> bool:
    """Reports whether an image file was written all the way through.

    The upscaler writes into the output directory as it goes, so a run that is
    interrupted can leave a frame half written. Such a frame has to be redone
    rather than counted as done.
    """
    terminator = _TERMINATORS.get(path.suffix.lower())
    if terminator is None:
        return path.stat().st_size > 0
    try:
        with open(path, "rb") as handle:
            if path.stat().st_size < len(terminator):
                return False
            handle.seek(-len(terminator), os.SEEK_END)
            return handle.read() == terminator
    except OSError:
        return False


def _stage_missing(source: Path, target: Path, suffix: str, staging: Path) -> list[Path]:
    """Links the frames with no complete output yet into a staging directory."""
    pending = []
    for frame in common.frame_files(source):
        output = target / f"{frame.stem}{suffix}"
        if output.is_file():
            if is_complete(output):
                continue
            output.unlink()
        link = staging / frame.name
        try:
            link.hardlink_to(frame)
        except (OSError, NotImplementedError):
            shutil.copy2(frame, link)
        pending.append(frame)
    return pending


class _Progress:
    """Reports how far a long directory run has got, by counting its output."""

    INTERVAL = 5.0

    def __init__(self, directory: Path, done: int, total: int):
        self.directory = directory
        self.done = done
        self.total = total
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._poll, daemon=True)

    def __enter__(self) -> "_Progress":
        self.started = time.monotonic()
        self._thread.start()
        return self

    def __exit__(self, *_exc) -> None:
        self._stop.set()
        self._thread.join(timeout=1.0)

    def _poll(self) -> None:
        while not self._stop.wait(self.INTERVAL):
            written = len(common.frame_files(self.directory)) - self.done
            if written <= 0:
                continue
            elapsed = time.monotonic() - self.started
            rate = written / elapsed
            remaining = (self.total - written) / rate if rate > 0 else 0
            common.log(
                f"    {written}/{self.total} frames, {rate:.2f} frames/s, "
                f"about {remaining / 60:.1f} min left"
            )


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
    # The animevideov3 models carry the scale in their filenames, so the name
    # passed to -n is not always the name on disk.
    stems = (config.upscale_model, f"{config.upscale_model}-x{config.upscale_factor}")
    if any(all((directory / f"{stem}{suffix}").is_file()
               for suffix in (".param", ".bin")) for stem in stems):
        return directory
    available = sorted({path.stem for path in directory.glob("*.param")})
    found = (f"Models in that folder: {', '.join(available)}."
             if available else "That folder holds no .param file at all.")
    wanted = " or ".join(f"{stem}.param with {stem}.bin" for stem in dict.fromkeys(stems))
    raise PipelineError(
        f"{directory} has no {wanted}. {found} {advice}"
    )


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


def upscale_dir(config: Config, source: Path, target: Path,
                backend: str = "realesrgan", fmt: str = "png",
                force: bool = False) -> tuple[Path, int, float]:
    """Enlarges every frame of one directory into another.

    Returns the target, how many frames this call ran, and the rate it managed.
    Frames already present and complete are left alone, so an interrupted run
    continues where it stopped.
    """
    if not common.frame_files(source):
        raise PipelineError(f"no frames in {source}; run dump_frames.py first")
    if force and target.is_dir():
        shutil.rmtree(common.guard_output(target))
    common.ensure_dir(target)

    suffix = f".{fmt}"
    # The staging directory sits beside the output so the frames can be linked
    # rather than copied, and so nothing crosses a drive.
    staging = common.ensure_dir(target.parent / f".staging-{target.name}")
    try:
        for stale in staging.iterdir():
            stale.unlink()
        pending = _stage_missing(source, target, suffix, staging)
        already = len(common.frame_files(target))
        if not pending:
            common.log(f"  upscale: {already} frames already present")
            return target, 0, 0.0
        common.log(f"  upscale: {len(pending)} frame(s) through {backend} -> {target}")
        started = time.monotonic()
        with _Progress(target, already, len(pending)):
            run_backend(config, staging, target, backend, fmt)
        elapsed = max(time.monotonic() - started, 1e-6)
        produced = len(common.frame_files(target)) - already
        if produced != len(pending):
            raise PipelineError(
                f"{target.name}: {backend} produced {produced} of {len(pending)} frames"
            )
    finally:
        shutil.rmtree(staging, ignore_errors=True)

    rate = len(pending) / elapsed
    common.log(f"  upscale: {len(pending)} frames in {elapsed:.1f}s ({rate:.2f} frames/s)")
    return target, len(pending), rate


def upscale(config: Config, name: str, backend: str = "realesrgan",
            use_denoise: bool = False, fmt: str = "png",
            force: bool = False) -> tuple[Path, int, float]:
    """Enlarges one movie's frames. Returns the directory, frames run and frames/s."""
    name = name.upper()
    source = denoise(config, name, force=force) if use_denoise else config.frames_dir(name)
    return upscale_dir(config, source, config.upscaled_dir(name),
                       backend=backend, fmt=fmt, force=force)


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
