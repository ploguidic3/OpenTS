"""Stage 1: copy the campaign VQAs out of the retail movie archives.

The archives index members by name checksum rather than by name, so extraction
asks for each name in the movie inventory instead of listing the archive. A
name held by more than one archive is taken from the first one searched, which
is the order the archives are named in the configuration.
"""

from __future__ import annotations

from pathlib import Path
import sys

import common
from common import PipelineError
import mixreader


def find_archives(data_dir: Path, names) -> list[Path]:
    """Returns the movie archives present in the data directory, in search order."""
    found = []
    for name in names:
        matches = [p for p in data_dir.iterdir()
                   if p.is_file() and p.name.lower() == name.lower()]
        found.extend(matches)
    return found


def extract(config: common.Config, data_dir: Path, movies, force: bool = False) -> dict:
    archives = find_archives(data_dir, config.movie_mixes)
    if not archives:
        raise PipelineError(
            f"none of {', '.join(config.movie_mixes)} is in {data_dir}. "
            "Point --data at the installed game directory."
        )
    destination = common.ensure_dir(config.work / "vqa")
    wanted = {f"{name}.VQA": name for name in movies}
    results: dict[str, Path] = {}
    for archive_path in archives:
        remaining = {n: m for n, m in wanted.items() if m not in results}
        if not remaining:
            break
        try:
            archive = mixreader.MixFile(archive_path)
        except mixreader.MixError as error:
            common.log(f"  {archive_path.name}: {error}")
            continue
        with archive:
            for member_name, member in archive.resolve(remaining).items():
                movie = remaining[member_name]
                target = common.guard_output(destination / f"{movie}.VQA")
                if target.is_file() and not force and target.stat().st_size == member.size:
                    results[movie] = target
                    continue
                target.write_bytes(archive.read_member(member))
                results[movie] = target
                common.log(f"  {archive_path.name} -> {movie}.VQA ({member.size} bytes)")
    return results


def main() -> int:
    parser = common.base_parser(__doc__.splitlines()[0])
    parser.add_argument("--data", type=Path, default=common.RUN_DIR,
                        help="installed game directory holding the movie archives")
    parser.add_argument("--rules", type=Path, default=None,
                        help="rules.ini to take the [Movies] inventory from")
    parser.add_argument("--force", action="store_true",
                        help="re-extract members that are already present")
    parser.add_argument("movies", nargs="*",
                        help="movie names to extract (default: the whole inventory)")
    args = parser.parse_args()
    config = common.apply_common(args)

    data_dir = args.data
    if not data_dir.is_dir():
        raise PipelineError(f"{data_dir} is not a directory")
    movies = [name.upper() for name in args.movies] or common.movie_names(args.rules)

    common.log(f"Extracting {len(movies)} movie name(s) from {data_dir}")
    results = extract(config, data_dir, movies, force=args.force)
    missing = [name for name in movies if name not in results]
    common.log(f"Extracted {len(results)} of {len(movies)}; {len(missing)} not present")
    if missing and args.verbose:
        common.log("  missing: " + ", ".join(missing))
    return 0


if __name__ == "__main__":
    sys.exit(common.main_guard(main))
