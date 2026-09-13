"""Stage 2: decode a movie to numbered PNG frames and a WAV sound track.

The frame count is checked against the decoder's own count, so a decode that
stops early is reported rather than carried into the upscale as a short movie.
"""

from __future__ import annotations

from pathlib import Path
import shutil
import sys

import common
from common import Config, MediaInfo, PipelineError


def source_for(config: Config, name: str, source: Path | None = None) -> Path:
    """Returns the file to decode: an explicit source, else the extracted VQA."""
    if source:
        if not source.is_file():
            raise PipelineError(f"{source} does not exist")
        return source
    path = config.vqa_file(name)
    if not path.is_file():
        raise PipelineError(
            f"{path} is not there. Run extract_movies.py first, or pass --source."
        )
    return path


def dump(config: Config, name: str, source: Path | None = None,
         force: bool = False) -> tuple[MediaInfo, Path, Path | None]:
    """Decodes one movie. Returns its media information, frame directory and WAV."""
    name = name.upper()
    origin = source_for(config, name, source)
    media = common.probe(origin)
    frames = config.frames_dir(name)

    if force and frames.is_dir():
        shutil.rmtree(common.guard_output(frames))
    present = common.frame_files(frames)
    if len(present) == media.frames and media.frames > 0:
        common.log(f"  frames: {media.frames} already present")
    else:
        if frames.is_dir():
            shutil.rmtree(common.guard_output(frames))
        common.ensure_dir(frames)
        common.run_ffmpeg([
            "-i", origin, "-fps_mode", "passthrough",
            "-pix_fmt", "rgb24", str(frames / "%05d.png"),
        ])
        present = common.frame_files(frames)
        if len(present) != media.frames:
            raise PipelineError(
                f"{name}: decoded {len(present)} frames but the decoder counted "
                f"{media.frames}. The frame directory is left in place for inspection."
            )
        common.log(f"  frames: {len(present)} written to {frames}")
    common.write_frame_info(frames, media)

    audio = None
    if media.has_audio:
        audio = config.audio_file(name)
        common.ensure_dir(audio.parent)
        if force or not audio.is_file():
            common.run_ffmpeg([
                "-i", origin, "-vn",
                "-acodec", "pcm_s16le",
                "-ar", str(config.audio_rate),
                "-ac", str(config.audio_channels),
                str(common.guard_output(audio)),
            ])
        common.log(f"  audio: {audio}")
    else:
        common.log("  audio: none in the source")
    return media, frames, audio


def main() -> int:
    parser = common.base_parser(__doc__.splitlines()[0])
    parser.add_argument("movie", help="movie name, without an extension")
    parser.add_argument("--source", type=Path, default=None,
                        help="decode this file instead of the extracted VQA")
    parser.add_argument("--force", action="store_true",
                        help="discard frames and sound already dumped")
    args = parser.parse_args()
    config = common.apply_common(args)

    common.log(f"Dumping {args.movie.upper()}")
    media, _frames, _audio = dump(config, args.movie, args.source, force=args.force)
    common.log(
        f"  {media.codec} {media.width}x{media.height} @ {media.fps_text} fps, "
        f"{media.frames} frames, {media.duration:.2f}s"
    )
    return 0


if __name__ == "__main__":
    sys.exit(common.main_guard(main))
