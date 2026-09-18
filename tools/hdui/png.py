"""Minimal PNG reader and writer.

Only what the pipeline needs: eight bits per channel, no interlacing. Images are
carried as ``Image``, a flat bytes buffer plus its mode, so that nothing here
depends on a third-party imaging library.
"""

from __future__ import annotations

import dataclasses
import struct
import zlib

SIGNATURE = b"\x89PNG\r\n\x1a\n"

GREY = "L"
RGB = "RGB"
RGBA = "RGBA"
PALETTE = "P"

_CHANNELS = {GREY: 1, RGB: 3, RGBA: 4, PALETTE: 1}
_COLOR_TYPE = {GREY: 0, RGB: 2, PALETTE: 3, RGBA: 6}
_MODE_FROM_COLOR = {0: GREY, 2: RGB, 3: PALETTE, 6: RGBA}


class PNGError(RuntimeError):
    pass


@dataclasses.dataclass
class Image:
    width: int
    height: int
    mode: str
    pixels: bytearray
    palette: list[tuple[int, int, int]] | None = None

    @property
    def channels(self) -> int:
        return _CHANNELS[self.mode]

    def pixel(self, x: int, y: int):
        step = self.channels
        start = (y * self.width + x) * step
        if step == 1:
            return self.pixels[start]
        return tuple(self.pixels[start:start + step])

    def row(self, y: int) -> bytes:
        stride = self.width * self.channels
        return bytes(self.pixels[y * stride:(y + 1) * stride])


def new(width: int, height: int, mode: str = RGB, fill: int = 0) -> Image:
    return Image(width, height, mode, bytearray([fill]) * (width * height * _CHANNELS[mode]))


def _chunk(kind: bytes, payload: bytes) -> bytes:
    return (struct.pack(">I", len(payload)) + kind + payload
            + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF))


def write(image: Image) -> bytes:
    """Encodes the image as PNG bytes, one filter-zero scanline per row."""
    color = _COLOR_TYPE[image.mode]
    out = bytearray(SIGNATURE)
    out += _chunk(b"IHDR", struct.pack(">IIBBBBB", image.width, image.height, 8, color, 0, 0, 0))

    if image.mode == PALETTE:
        if not image.palette:
            raise PNGError("a palette image needs a palette")
        table = bytearray()
        for red, green, blue in image.palette:
            table += bytes((red, green, blue))
        out += _chunk(b"PLTE", bytes(table.ljust(768, b"\0")))

    stride = image.width * image.channels
    raw = bytearray()
    for y in range(image.height):
        raw.append(0)
        raw += image.pixels[y * stride:(y + 1) * stride]

    out += _chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    out += _chunk(b"IEND", b"")
    return bytes(out)


def _unfilter(raw: bytes, width: int, height: int, channels: int) -> bytearray:
    stride = width * channels
    out = bytearray(stride * height)
    previous = bytearray(stride)
    position = 0

    for y in range(height):
        method = raw[position]
        position += 1
        line = bytearray(raw[position:position + stride])
        position += stride
        if len(line) < stride:
            raise PNGError("the image data stops short")

        if method == 1:
            for i in range(channels, stride):
                line[i] = (line[i] + line[i - channels]) & 0xFF
        elif method == 2:
            for i in range(stride):
                line[i] = (line[i] + previous[i]) & 0xFF
        elif method == 3:
            for i in range(stride):
                left = line[i - channels] if i >= channels else 0
                line[i] = (line[i] + ((left + previous[i]) >> 1)) & 0xFF
        elif method == 4:
            for i in range(stride):
                left = line[i - channels] if i >= channels else 0
                up = previous[i]
                upleft = previous[i - channels] if i >= channels else 0
                estimate = left + up - upleft
                da, db, dc = abs(estimate - left), abs(estimate - up), abs(estimate - upleft)
                nearest = left if da <= db and da <= dc else (up if db <= dc else upleft)
                line[i] = (line[i] + nearest) & 0xFF
        elif method != 0:
            raise PNGError(f"filter {method} is not a PNG filter")

        out[y * stride:(y + 1) * stride] = line
        previous = line

    return out


def read(data: bytes) -> Image:
    """Decodes eight-bit, non-interlaced PNG bytes."""
    if not data.startswith(SIGNATURE):
        raise PNGError("not a PNG file")

    position = len(SIGNATURE)
    width = height = depth = color = 0
    palette: list[tuple[int, int, int]] | None = None
    compressed = bytearray()

    while position + 8 <= len(data):
        length, kind = struct.unpack_from(">I4s", data, position)
        position += 8
        payload = data[position:position + length]
        position += length + 4

        if kind == b"IHDR":
            width, height, depth, color, _comp, _filt, interlace = struct.unpack(">IIBBBBB", payload)
            if depth != 8:
                raise PNGError(f"{depth} bits per channel is not supported")
            if interlace:
                raise PNGError("interlaced images are not supported")
            if color not in _MODE_FROM_COLOR:
                raise PNGError(f"color type {color} is not supported")
        elif kind == b"PLTE":
            palette = [tuple(payload[i:i + 3]) for i in range(0, len(payload), 3)]
        elif kind == b"IDAT":
            compressed += payload
        elif kind == b"IEND":
            break

    if width <= 0 or height <= 0:
        raise PNGError("the image has no size")

    mode = _MODE_FROM_COLOR[color]
    pixels = _unfilter(zlib.decompress(bytes(compressed)), width, height, _CHANNELS[mode])
    return Image(width, height, mode, pixels, palette)
