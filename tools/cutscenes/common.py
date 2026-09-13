"""Shared helpers for the cutscene upscale pipeline.

Tool discovery, media probing, the work tree layout and the run manifest live
here so the four stage scripts and ``pipeline.py`` agree on all of them.
"""

from __future__ import annotations

import argparse
import contextlib
import dataclasses
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[2]
TOOLS = Path(__file__).resolve().parent
DEFAULT_CONFIG = TOOLS / "config.json"

# Retail game data. The pipeline reads from here and must never write into it.
RUN_DIR = ROOT / "Run"


class PipelineError(RuntimeError):
    """A stage could not continue. Carries a message meant for the operator."""


# --------------------------------------------------------------------------
# Configuration


@dataclasses.dataclass(frozen=True)
class Config:
    work: Path
    out: Path
    target_width: int
    target_height: int
    upscale_model: str
    upscale_factor: int
    gpu: str
    threads: str
    model_path: str
    crf: int
    preset: str
    audio_bitrate: str
    audio_rate: int
    audio_channels: int
    preview_frames: int
    denoise_filter: str
    post_filter: str
    trial_frames: int
    trials: tuple
    optional_movies: tuple[str, ...]
    movie_mixes: tuple[str, ...]

    @classmethod
    def load(cls, path: Path | None = None) -> "Config":
        path = Path(path) if path else DEFAULT_CONFIG
        data = json.loads(path.read_text(encoding="utf-8"))
        work = Path(data["work"])
        out = Path(data["out"])
        return cls(
            work=work if work.is_absolute() else (TOOLS / work).resolve(),
            out=out if out.is_absolute() else (TOOLS / out).resolve(),
            target_width=int(data["target_width"]),
            target_height=int(data["target_height"]),
            upscale_model=data["upscale_model"],
            upscale_factor=int(data["upscale_factor"]),
            gpu=str(data["gpu"]),
            threads=str(data["threads"]),
            model_path=str(data.get("model_path", "")),
            crf=int(data["crf"]),
            preset=data["preset"],
            audio_bitrate=data["audio_bitrate"],
            audio_rate=int(data["audio_rate"]),
            audio_channels=int(data["audio_channels"]),
            preview_frames=int(data["preview_frames"]),
            denoise_filter=data["denoise_filter"],
            post_filter=str(data.get("post_filter", "")),
            trial_frames=int(data.get("trial_frames", 120)),
            trials=tuple(data.get("trials", ())),
            optional_movies=tuple(n.upper() for n in data["optional_movies"]),
            movie_mixes=tuple(data["movie_mixes"]),
        )

    def frames_dir(self, name: str) -> Path:
        return self.work / "frames" / name.upper()

    def denoised_dir(self, name: str) -> Path:
        return self.work / "denoised" / name.upper()

    def upscaled_dir(self, name: str) -> Path:
        return self.work / "up" / name.upper()

    def interpolated_dir(self, name: str) -> Path:
        return self.work / "rife" / name.upper()

    def audio_file(self, name: str) -> Path:
        return self.work / "audio" / f"{name.upper()}.wav"

    def vqa_file(self, name: str) -> Path:
        return self.work / "vqa" / f"{name.upper()}.VQA"

    def output_file(self, name: str) -> Path:
        return self.out / f"{name.upper()}.mp4"

    def trial_dir(self, name: str) -> Path:
        return self.work / "trial" / name.upper()

    def compare_dir(self) -> Path:
        return self.work / "compare"

    def manifest_file(self) -> Path:
        return self.work / "manifest.json"


# --------------------------------------------------------------------------
# Tool discovery


def _from_env(var: str) -> Path | None:
    value = os.environ.get(var)
    if not value:
        return None
    path = Path(value)
    if not path.exists():
        raise PipelineError(f"{var} points at {path}, which does not exist")
    return path


def find_tool(name: str, env_var: str, required: bool = True) -> Path | None:
    """Locates an external executable, preferring the environment override.

    Raises PipelineError naming the environment variable when a required tool
    is missing, so the operator is told how to supply it.
    """
    override = _from_env(env_var)
    if override:
        return override
    found = shutil.which(name)
    if found:
        return Path(found)
    if not required:
        return None
    raise PipelineError(
        f"{name} was not found on PATH. Install it or set {env_var} to its full path."
    )


def ffmpeg(required: bool = True) -> Path | None:
    return find_tool("ffmpeg", "OPENTS_FFMPEG", required)


def ffprobe(required: bool = False) -> Path | None:
    return find_tool("ffprobe", "OPENTS_FFPROBE", required)


def realesrgan(required: bool = True) -> Path | None:
    return find_tool("realesrgan-ncnn-vulkan", "OPENTS_REALESRGAN", required)


def rife(required: bool = True) -> Path | None:
    return find_tool("rife-ncnn-vulkan", "OPENTS_RIFE", required)


# --------------------------------------------------------------------------
# Process running


VERBOSE = False


def log(message: str) -> None:
    print(message, flush=True)


def run(command, capture: bool = False, check: bool = True) -> subprocess.CompletedProcess:
    """Runs a child process, echoing the command line when --verbose is set."""
    printable = [str(part) for part in command]
    if VERBOSE:
        log("  $ " + " ".join(printable))
    result = subprocess.run(
        printable,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.PIPE if capture else None,
        text=True,
    )
    if check and result.returncode != 0:
        detail = (result.stderr or "").strip()[-2000:]
        raise PipelineError(
            f"{printable[0]} exited {exit_code_text(result.returncode)}"
            + (f"\n{detail}" if detail else "")
        )
    return result


def exit_code_text(code: int) -> str:
    """Renders an exit code, adding hex for the Windows status codes."""
    if code > 0xFFFF or code < -1:
        return f"{code} (0x{code & 0xFFFFFFFF:08X})"
    return str(code)


def run_ffmpeg(args, check: bool = True) -> subprocess.CompletedProcess:
    return run([ffmpeg(), "-hide_banner", "-nostdin", "-y", *args], capture=True, check=check)


# --------------------------------------------------------------------------
# Media probing


@dataclasses.dataclass(frozen=True)
class MediaInfo:
    width: int
    height: int
    fps: float
    frames: int
    codec: str
    has_audio: bool
    duration: float
    counted: bool  # True when `frames` came from decoding rather than a header field.

    @property
    def fps_text(self) -> str:
        return f"{self.fps:g}"


_STREAM_RE = re.compile(
    r"Stream #\d+:\d+.*?: Video: (?P<codec>[\w]+).*?, (?P<w>\d+)x(?P<h>\d+)[ ,]"
)
_FPS_RE = re.compile(r"(?P<fps>[\d.]+) (?:fps|tbr)")
_DURATION_RE = re.compile(r"Duration: (\d+):(\d+):(\d+\.\d+)")
_FRAME_RE = re.compile(r"frame=\s*(\d+)")


def probe(path: Path) -> MediaInfo:
    """Reports the first video stream's geometry, rate and exact frame count.

    ffprobe is used when present. Its ``nb_frames`` is absent for VQA, so the
    count always comes from ``-count_frames``. Without ffprobe the same numbers
    are read back from an ffmpeg null-decode, which costs one extra decode pass.
    """
    path = Path(path)
    if not path.is_file():
        raise PipelineError(f"{path} does not exist")
    probe_tool = ffprobe(required=False)
    if probe_tool:
        return _probe_with_ffprobe(probe_tool, path)
    return _probe_with_ffmpeg(path)


def _probe_with_ffprobe(probe_tool: Path, path: Path) -> MediaInfo:
    result = run(
        [
            probe_tool, "-v", "error", "-count_frames",
            "-select_streams", "v:0",
            "-show_entries",
            "stream=width,height,avg_frame_rate,r_frame_rate,codec_name,nb_read_frames,nb_frames",
            "-show_entries", "format=duration",
            "-of", "json", path,
        ],
        capture=True,
    )
    data = json.loads(result.stdout)
    streams = data.get("streams") or []
    if not streams:
        raise PipelineError(f"{path} carries no video stream")
    stream = streams[0]
    rate = stream.get("avg_frame_rate") or stream.get("r_frame_rate") or "0/1"
    if rate in ("0/0", "0/1"):
        rate = stream.get("r_frame_rate", "0/1")
    numerator, _, denominator = rate.partition("/")
    fps = float(numerator) / float(denominator or 1) if float(denominator or 1) else 0.0
    frames = stream.get("nb_read_frames") or stream.get("nb_frames") or "0"
    audio = run(
        [probe_tool, "-v", "error", "-select_streams", "a", "-show_entries",
         "stream=index", "-of", "csv=p=0", path],
        capture=True,
    )
    duration = data.get("format", {}).get("duration") or "0"
    return MediaInfo(
        width=int(stream["width"]),
        height=int(stream["height"]),
        fps=fps,
        frames=int(frames) if str(frames).isdigit() else 0,
        codec=stream.get("codec_name", "?"),
        has_audio=bool(audio.stdout.strip()),
        duration=float(duration) if duration not in ("N/A", "") else 0.0,
        counted=True,
    )


def _probe_with_ffmpeg(path: Path) -> MediaInfo:
    result = run_ffmpeg(["-i", path, "-map", "0:v:0", "-f", "null", "-"], check=False)
    text = (result.stderr or "") + (result.stdout or "")
    stream = _STREAM_RE.search(text)
    if not stream:
        raise PipelineError(f"ffmpeg reported no video stream in {path}:\n{text[-1500:]}")
    line_end = text.find("\n", stream.end())
    line = text[stream.start(): line_end if line_end > 0 else len(text)]
    fps_match = _FPS_RE.search(line)
    duration_match = _DURATION_RE.search(text)
    duration = 0.0
    if duration_match:
        hours, minutes, seconds = duration_match.groups()
        duration = int(hours) * 3600 + int(minutes) * 60 + float(seconds)
    counts = _FRAME_RE.findall(text)
    return MediaInfo(
        width=int(stream.group("w")),
        height=int(stream.group("h")),
        fps=float(fps_match.group("fps")) if fps_match else 0.0,
        frames=int(counts[-1]) if counts else 0,
        codec=stream.group("codec"),
        has_audio="Audio:" in text,
        duration=duration,
        counted=True,
    )


# --------------------------------------------------------------------------
# Work tree helpers


def guard_output(path: Path) -> Path:
    """Refuses a destination inside Run/, which holds the retail install."""
    resolved = Path(path).resolve()
    with contextlib.suppress(ValueError):
        resolved.relative_to(RUN_DIR.resolve())
        raise PipelineError(f"refusing to write into the retail data directory: {resolved}")
    return resolved


def ensure_dir(path: Path) -> Path:
    path = guard_output(path)
    path.mkdir(parents=True, exist_ok=True)
    return path


def frame_files(directory: Path) -> list[Path]:
    """Returns the stage's frame images in numeric order, whatever their suffix."""
    if not Path(directory).is_dir():
        return []
    return sorted(
        p for p in Path(directory).iterdir()
        if p.suffix.lower() in (".png", ".jpg", ".jpeg") and p.stem.isdigit()
    )


def frame_pattern(directory: Path) -> str:
    """Returns the ffmpeg input pattern matching the frames already in place."""
    files = frame_files(directory)
    suffix = files[0].suffix if files else ".png"
    return str(Path(directory) / f"%05d{suffix}")


INFO_NAME = ".info.json"


def write_frame_info(directory: Path, media: "MediaInfo") -> Path:
    """Records a dumped movie's rate and geometry beside its frames.

    Later stages read this rather than the source, which they may not have.
    """
    path = guard_output(Path(directory) / INFO_NAME)
    path.write_text(json.dumps(dataclasses.asdict(media), indent=2) + "\n",
                    encoding="utf-8")
    return path


def read_frame_info(directory: Path) -> MediaInfo | None:
    """Returns what was recorded for a frame directory, or None if nothing was."""
    path = Path(directory) / INFO_NAME
    if not path.is_file():
        return None
    try:
        return MediaInfo(**json.loads(path.read_text(encoding="utf-8")))
    except (ValueError, TypeError):
        return None


def frame_rate(directory: Path, fallback: float = 15.0) -> float:
    """Returns the recorded frame rate of a frame directory, or the fallback."""
    info = read_frame_info(directory)
    return info.fps if info and info.fps else fallback


def md5_of(path: Path) -> str:
    digest = hashlib.md5()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


# --------------------------------------------------------------------------
# Movie name inventory


_ENUM_RE = re.compile(r"^\s*VQ_([A-Z0-9_]+)\s*[,=]", re.MULTILINE)
_SKIP_ENUMERATORS = {"NONE", "COUNT", "FIRST"}


def movie_names(rules_ini: Path | None = None) -> list[str]:
    """Returns the campaign movie names, from rules.ini when it is available.

    ``rules.ini [Movies]`` is the table the game actually loads; its order must
    match ``code/vq.hh``. Without the retail file the enumerator names stand in,
    which is the same inventory under the same names.
    """
    if rules_ini and Path(rules_ini).is_file():
        names = _movies_from_rules(Path(rules_ini))
        if names:
            return names
    return _movies_from_enum(ROOT / "code" / "vq.hh")


def _movies_from_rules(path: Path) -> list[str]:
    names: list[str] = []
    in_section = False
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.split(";", 1)[0].strip()
        if not line:
            continue
        if line.startswith("["):
            in_section = line.lower() == "[movies]"
            continue
        if in_section and "=" in line:
            names.append(line.split("=", 1)[1].strip().upper())
    return names


def _movies_from_enum(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8", errors="replace")
    return [n for n in _ENUM_RE.findall(text) if n not in _SKIP_ENUMERATORS]


# --------------------------------------------------------------------------
# Manifest


def load_manifest(config: Config) -> dict:
    path = config.manifest_file()
    if path.is_file():
        return json.loads(path.read_text(encoding="utf-8"))
    return {"movies": {}}


def save_manifest(config: Config, manifest: dict) -> None:
    path = guard_output(config.manifest_file())
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def record(manifest: dict, name: str, **fields) -> dict:
    entry = manifest.setdefault("movies", {}).setdefault(name.upper(), {})
    entry.update(fields)
    entry["updated"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    return entry


# --------------------------------------------------------------------------
# Argument parsing


def base_parser(description: str) -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG,
                        help="pipeline configuration file (default: config.json)")
    parser.add_argument("--verbose", action="store_true",
                        help="echo every external command line")
    return parser


def apply_common(args) -> Config:
    global VERBOSE
    VERBOSE = bool(getattr(args, "verbose", False))
    return Config.load(args.config)


def main_guard(entry) -> int:
    """Runs a stage's main and turns PipelineError into a one-line failure."""
    try:
        return entry() or 0
    except PipelineError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("interrupted", file=sys.stderr)
        return 130
