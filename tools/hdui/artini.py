"""Lists the cameo shapes an art file names.

The engine takes a cameo from the `Cameo=` entry of a type's art section, and
draws `XXICON.SHP` for a type with none (code/techtype.cpp). Every section is
read, since a type falls back to its own section's entry when its graphic's
section has none.
"""

from __future__ import annotations

import argparse
from pathlib import Path

FALLBACK = "XXICON.SHP"


def cameo_names(text: str) -> list[str]:
    """Returns each shape named by a Cameo= entry, once, in file order, with the fallback."""
    names = [FALLBACK]
    for line in text.splitlines():
        line = line.split(";", 1)[0]
        key, sep, value = line.partition("=")
        if not sep or key.strip().lower() != "cameo":
            continue
        value = value.strip().upper()
        if not value:
            continue
        name = value if value.endswith(".SHP") else value + ".SHP"
        if name not in names:
            names.append(name)
    return names


def read_text(data: bytes) -> str:
    return data.decode("latin-1")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("art", type=Path, nargs="+", help="an art file, such as ART.INI")
    args = parser.parse_args(argv)

    names: list[str] = []
    for path in args.art:
        for name in cameo_names(read_text(path.read_bytes())):
            if name not in names:
                names.append(name)
    for name in names:
        print(name)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
