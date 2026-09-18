"""Pulls named members out of MIX archives.

The index holds a checksum rather than a name, so only names asked for can be
found. The archives are searched in the order given, and the first one holding a
name answers for it, which is the order the engine mounts them in.
"""

from __future__ import annotations

import argparse
from pathlib import Path

import mixreader


def extract(archives, names, out_dir: Path) -> dict[str, Path]:
    """Writes each name found to out_dir and returns what was written."""
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    wanted = list(names)
    written: dict[str, Path] = {}

    for archive in archives:
        if not wanted:
            break
        with mixreader.MixFile(Path(archive)) as mix:
            found = mix.resolve(wanted)
            for name, member in found.items():
                path = out_dir / name.upper()
                path.write_bytes(mix.read_member(member))
                written[name] = path
            wanted = [name for name in wanted if name not in found]

    return written


def _names_from(path: Path) -> list[str]:
    lines = Path(path).read_text().splitlines()
    return [line.strip() for line in lines if line.strip() and not line.startswith("#")]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("out_dir", type=Path, help="where the members are written")
    parser.add_argument("--mix", type=Path, action="append", required=True,
                        help="an archive to search, earliest first; repeat for more")
    parser.add_argument("--names", type=Path, help="a file of member names, one per line")
    parser.add_argument("--name", action="append", default=[], help="a member name; repeat for more")
    args = parser.parse_args(argv)

    names = list(args.name)
    if args.names:
        names += _names_from(args.names)
    if not names:
        parser.error("name at least one member with --name or --names")

    written = extract(args.mix, names, args.out_dir)
    for name in names:
        print(f"{name}: {written[name] if name in written else 'not in any archive given'}")

    return 0 if len(written) == len(names) else 1


if __name__ == "__main__":
    raise SystemExit(main())
