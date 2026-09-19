"""Reader and writer for the SHP shape files the engine draws interface art from.

The layout is the one ``code/shapeset.h`` casts over the file bytes: an eight
byte header, one twenty-four byte record per frame, then the frame data. A frame
is either a flat block of palette indices or rows of zero-run encoding, each row
prefixed with its own byte length, as ``RLE_Blit`` in ``code/blit.cpp`` reads it.
"""

from __future__ import annotations

import dataclasses
import struct

HEADER_SIZE = 8
RECORD_SIZE = 24
DATA_OFFSET = 20

FLAG_TRANSPARENT = 0x01
FLAG_RLE = 0x02


class ShapeError(RuntimeError):
    pass


@dataclasses.dataclass
class Frame:
    x: int = 0
    y: int = 0
    width: int = 0
    height: int = 0
    flags: int = 0
    color: tuple[int, int, int] = (0, 0, 0)
    pixels: bytes = b""          # width * height palette indices, row major.

    @property
    def is_rle(self) -> bool:
        return bool(self.flags & FLAG_RLE)

    @property
    def is_transparent(self) -> bool:
        return bool(self.flags & FLAG_TRANSPARENT)


@dataclasses.dataclass
class Shape:
    width: int = 0               # The logical box every frame is placed within.
    height: int = 0
    flags: int = 0
    frames: list[Frame] = dataclasses.field(default_factory=list)


def _decode_rle(data: bytes, offset: int, width: int, height: int) -> bytes:
    out = bytearray(width * height)
    position = offset

    for y in range(height):
        if position + 2 > len(data):
            raise ShapeError("a compressed frame ends before its rows do")
        length = struct.unpack_from("<H", data, position)[0]
        if length < 2 or position + length > len(data):
            raise ShapeError(f"a row claims {length} bytes, which the file does not hold")

        stop = position + length
        read = position + 2
        written = 0
        while read < stop and written < width:
            value = data[read]
            if value == 0:
                if read + 1 >= stop:
                    raise ShapeError("a run of transparent pixels has no count")
                written += data[read + 1]
                read += 2
            else:
                out[y * width + written] = value
                written += 1
                read += 1
        position += length

    return bytes(out)


def _encode_rle(pixels: bytes, width: int, height: int) -> bytes:
    out = bytearray()

    for y in range(height):
        row = pixels[y * width:(y + 1) * width]
        body = bytearray()
        index = 0
        while index < width:
            if row[index] == 0:
                run = 0
                while index + run < width and row[index + run] == 0 and run < 255:
                    run += 1
                body += bytes((0, run))
                index += run
            else:
                body.append(row[index])
                index += 1
        out += struct.pack("<H", len(body) + 2) + body

    return bytes(out)


def read(data: bytes) -> Shape:
    if len(data) < HEADER_SIZE:
        raise ShapeError("too short to hold a shape header")

    flags, width, height, count = struct.unpack_from("<hhhh", data, 0)
    if count < 0 or len(data) < HEADER_SIZE + count * RECORD_SIZE:
        raise ShapeError(f"the file declares {count} frames but its records stop short")

    shape = Shape(width=width, height=height, flags=flags)

    for index in range(count):
        base = HEADER_SIZE + index * RECORD_SIZE
        fx, fy, fwidth, fheight, fflags, _size = struct.unpack_from("<hhhhhH", data, base)
        color = tuple(data[base + 12:base + 15])
        offset = struct.unpack_from("<i", data, base + DATA_OFFSET)[0]

        frame = Frame(fx, fy, fwidth, fheight, fflags, color)
        if offset and fwidth > 0 and fheight > 0:
            if fflags & FLAG_RLE:
                frame.pixels = _decode_rle(data, offset, fwidth, fheight)
            else:
                needed = fwidth * fheight
                if offset + needed > len(data):
                    raise ShapeError("a frame's pixels run past the end of the file")
                frame.pixels = bytes(data[offset:offset + needed])
        shape.frames.append(frame)

    return shape


def write(shape: Shape) -> bytes:
    count = len(shape.frames)
    encoded = []

    for frame in shape.frames:
        if not frame.pixels:
            encoded.append(b"")
        elif frame.is_rle:
            encoded.append(_encode_rle(frame.pixels, frame.width, frame.height))
        else:
            if len(frame.pixels) != frame.width * frame.height:
                raise ShapeError("a frame carries a different number of pixels than its size")
            encoded.append(bytes(frame.pixels))

    out = bytearray(struct.pack("<hhhh", shape.flags, shape.width, shape.height, count))
    offset = HEADER_SIZE + count * RECORD_SIZE

    for frame, body in zip(shape.frames, encoded):
        # The field is sixteen bits, which an enlarged backdrop outgrows. The engine finds
        # a frame by its offset and never reads the size, so one too large to describe
        # records zero rather than a truncated number that would read as valid.
        described = len(body) if len(body) <= 0xFFFF else 0

        out += struct.pack("<hhhhhH", frame.x, frame.y, frame.width, frame.height,
                           frame.flags, described)
        out += bytes(frame.color[:3]).ljust(3, b"\0")
        out += b"\0" * 5
        out += struct.pack("<i", offset if body else 0)
        offset += len(body)

    for body in encoded:
        out += body

    return bytes(out)


def magnify(shape: Shape, factor: int) -> Shape:
    """Grows every pixel to a square of the factor, offsets and all.

    This is the nearest-neighbour fallback; the pipeline replaces the pixels of
    each frame with upscaled ones and keeps these offsets.
    """
    if factor < 1:
        raise ShapeError("the factor must be at least one")
    if factor == 1:
        return shape

    grown = Shape(shape.width * factor, shape.height * factor, shape.flags)

    for frame in shape.frames:
        big = Frame(frame.x * factor, frame.y * factor, frame.width * factor,
                    frame.height * factor, frame.flags, frame.color)
        if frame.pixels:
            rows = bytearray()
            for y in range(frame.height):
                row = frame.pixels[y * frame.width:(y + 1) * frame.width]
                wide = bytes(value for value in row for _ in range(factor))
                rows += wide * factor
            big.pixels = bytes(rows)
        grown.frames.append(big)

    return grown
