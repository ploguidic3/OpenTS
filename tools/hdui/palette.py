"""Palette loading and nearest-colour matching.

A `.PAL` file is 256 six-bit RGB triples. The engine widens them to eight bits by
multiplying by four (`SidebarClass::Init_For_House`), so the same widening is
done here and the match is made in CIE L*a*b*, where distance tracks what the eye
sees far better than it does in RGB.
"""

from __future__ import annotations

import dataclasses
from pathlib import Path

COLOR_COUNT = 256

# D65, the white point sRGB is defined against.
_WHITE = (95.047, 100.0, 108.883)


class PaletteError(RuntimeError):
    pass


def _linear(value: int) -> float:
    channel = value / 255.0
    return channel / 12.92 if channel <= 0.04045 else ((channel + 0.055) / 1.055) ** 2.4


def _lab(color: tuple[int, int, int]) -> tuple[float, float, float]:
    red, green, blue = (_linear(part) * 100.0 for part in color)

    x = (red * 0.4124 + green * 0.3576 + blue * 0.1805) / _WHITE[0]
    y = (red * 0.2126 + green * 0.7152 + blue * 0.0722) / _WHITE[1]
    z = (red * 0.0193 + green * 0.1192 + blue * 0.9505) / _WHITE[2]

    def shape(value: float) -> float:
        return value ** (1.0 / 3.0) if value > 0.008856 else (7.787 * value) + (16.0 / 116.0)

    fx, fy, fz = shape(x), shape(y), shape(z)
    return (116.0 * fy - 16.0, 500.0 * (fx - fy), 200.0 * (fy - fz))


@dataclasses.dataclass
class Palette:
    colors: list[tuple[int, int, int]]

    def __post_init__(self) -> None:
        if len(self.colors) != COLOR_COUNT:
            raise PaletteError(f"a palette holds {COLOR_COUNT} colors, not {len(self.colors)}")
        self._lab = [_lab(color) for color in self.colors]
        self._cache: dict[tuple[tuple[int, int, int], tuple[int, ...]], int] = {}

    @classmethod
    def load(cls, path: Path) -> "Palette":
        data = Path(path).read_bytes()
        if len(data) < COLOR_COUNT * 3:
            raise PaletteError(f"{path} is {len(data)} bytes, too short for a palette")
        return cls([(data[i * 3] * 4, data[i * 3 + 1] * 4, data[i * 3 + 2] * 4)
                    for i in range(COLOR_COUNT)])

    def save(self, path: Path) -> Path:
        out = bytearray()
        for red, green, blue in self.colors:
            out += bytes((red // 4, green // 4, blue // 4))
        Path(path).write_bytes(bytes(out))
        return Path(path)

    def nearest(self, color: tuple[int, int, int], among: tuple[int, ...] | None = None) -> int:
        """Returns the index of the closest colour, searched among the given indices."""
        candidates = among if among is not None else tuple(range(1, COLOR_COUNT))
        key = (color, candidates)
        found = self._cache.get(key)
        if found is not None:
            return found

        want = _lab(color)
        best = candidates[0]
        best_distance = None
        for index in candidates:
            have = self._lab[index]
            distance = ((want[0] - have[0]) ** 2 + (want[1] - have[1]) ** 2
                        + (want[2] - have[2]) ** 2)
            if best_distance is None or distance < best_distance:
                best, best_distance = index, distance

        self._cache[key] = best
        return best
