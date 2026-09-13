"""Stage 4: encode the upscaled frames and the dumped sound into one MP4.

The sound is written at the configured rate in stereo rather than at the VQA's
own 22050 Hz mono: the Windows decoder the engine plays these files with
mishandles that rate, which shows up as a stuttering picture.
"""

from __future__ import annotations

from pathlib import Path
import sys

import common
from common import Config, PipelineError


def encode(config: Config, name: str, frames: Path | None = None,
           fps: float = 15.0, audio: Path | None = None,
           preview: bool = False, force: bool = False,
           post_filter: str | None = None, target: Path | None = None) -> Path:
    """Encodes one movie. Returns the output path."""
    name = name.upper()
    frames = Path(frames) if frames else config.upscaled_dir(name)
    if not common.frame_files(frames):
        raise PipelineError(f"no frames in {frames}; run upscale.py first")
    if audio is None:
        candidate = config.audio_file(name)
        audio = candidate if candidate.is_file() else None

    target = Path(target) if target else config.output_file(name)
    if preview:
        target = target.with_name(f"{target.stem}-preview.mp4")
    common.ensure_dir(target.parent)
    if target.is_file() and not force:
        common.log(f"  encode: {target.name} already present")
        return target

    args = ["-framerate", f"{fps:g}", "-i", common.frame_pattern(frames)]
    if audio:
        args += ["-i", str(audio)]
    chain = f"scale={config.target_width}:{config.target_height}:flags=lanczos"
    post = config.post_filter if post_filter is None else post_filter
    if post:
        chain += f",{post}"
    args += [
        "-vf", chain,
        "-c:v", "libx264",
        "-preset", config.preset,
        "-crf", str(config.crf),
        "-pix_fmt", "yuv420p",
    ]
    if audio:
        args += [
            "-c:a", "aac",
            "-b:a", config.audio_bitrate,
            "-ar", str(config.audio_rate),
            "-ac", str(config.audio_channels),
        ]
    if preview:
        args += ["-frames:v", str(config.preview_frames), "-shortest"]
    args += ["-movflags", "+faststart", str(common.guard_output(target))]

    common.run_ffmpeg(args)
    size = target.stat().st_size
    common.log(f"  encode: {target} ({size / (1 << 20):.1f} MiB)")
    return target


def main() -> int:
    parser = common.base_parser(__doc__.splitlines()[0])
    parser.add_argument("movie", help="movie name, without an extension")
    parser.add_argument("--frames", type=Path, default=None,
                        help="frame directory to encode (default: the upscaled frames)")
    parser.add_argument("--fps", type=float, default=None,
                        help="frame rate (default: the rate the frames were dumped at)")
    parser.add_argument("--audio", type=Path, default=None,
                        help="sound track to mux (default: the dumped WAV, if any)")
    parser.add_argument("--post-filter", dest="post_filter", default=None,
                        help="ffmpeg filter chain applied after the scale")
    parser.add_argument("--preview", action="store_true",
                        help="encode only the first frames, to a -preview.mp4")
    parser.add_argument("--force", action="store_true",
                        help="re-encode over an output that is already there")
    args = parser.parse_args()
    config = common.apply_common(args)

    common.log(f"Encoding {args.movie.upper()}")
    fps = args.fps or common.frame_rate(config.frames_dir(args.movie))
    encode(config, args.movie, frames=args.frames, fps=fps, audio=args.audio,
           preview=args.preview, force=args.force, post_filter=args.post_filter)
    return 0


if __name__ == "__main__":
    sys.exit(common.main_guard(main))
