"""Reader and writer for the legacy .FNT bitmap fonts.

The layout is the one ``WWFontClass`` reads (``code/wwfont.h``, ``Print`` in
``code/wwfont.cpp``): a sixteen byte header naming four blocks, then a glyph
offset, a width, a height pair and the packed pixels for each glyph. A glyph's
pixels are palette indices packed two to a byte, low nibble first, with
``(width + 1) // 2`` bytes per row and only the rows that carry ink.
"""

from __future__ import annotations

import dataclasses
import struct

HEADER_SIZE = 16
INFO_SIZE = 6
INFO_MAX_HEIGHT = 4
INFO_MAX_WIDTH = 5

COMPRESS_NEW = 2


class FontError(RuntimeError):
    pass


@dataclasses.dataclass
class Glyph:
    width: int = 0
    first_row: int = 0           # Blank rows above the ink.
    height: int = 0              # Rows of ink.
    pixels: bytes = b""          # width * height indices of 0..15, row major.


@dataclasses.dataclass
class Font:
    max_width: int = 0
    max_height: int = 0
    glyphs: list[Glyph] = dataclasses.field(default_factory=list)


def _unpack_rows(data: bytes, offset: int, width: int, height: int) -> bytes:
    stride = (width + 1) // 2
    out = bytearray(width * height)

    for y in range(height):
        start = offset + y * stride
        if start + stride > len(data):
            raise FontError("a glyph's pixels run past the end of the file")
        for x in range(width):
            packed = data[start + (x >> 1)]
            out[y * width + x] = (packed & 0x0F) if (x & 1) == 0 else (packed >> 4)

    return bytes(out)


def _pack_rows(pixels: bytes, width: int, height: int) -> bytes:
    stride = (width + 1) // 2
    out = bytearray(stride * height)

    for y in range(height):
        for x in range(width):
            value = pixels[y * width + x] & 0x0F
            index = y * stride + (x >> 1)
            if (x & 1) == 0:
                out[index] |= value
            else:
                out[index] |= value << 4

    return bytes(out)


def read(data: bytes) -> Font:
    if len(data) < HEADER_SIZE:
        raise FontError("too short to hold a font header")

    (_length, compress, _blocks, info_offset, offset_block,
     width_block, data_block, height_block) = struct.unpack_from("<HBBHHHHH", data, 0)

    count = (width_block - offset_block) // 2
    if count <= 0:
        raise FontError("the font declares no glyphs")
    if info_offset + INFO_SIZE > len(data):
        raise FontError("the information block is outside the file")

    font = Font(max_width=data[info_offset + INFO_MAX_WIDTH],
                max_height=data[info_offset + INFO_MAX_HEIGHT])

    for index in range(count):
        offset = struct.unpack_from("<H", data, offset_block + index * 2)[0]
        if compress == COMPRESS_NEW:
            offset += data_block

        width = data[width_block + index]
        packed = struct.unpack_from("<H", data, height_block + index * 2)[0]
        first_row = packed & 0xFF
        height = packed >> 8

        glyph = Glyph(width, first_row, height)
        if width > 0 and height > 0:
            glyph.pixels = _unpack_rows(data, offset, width, height)
        font.glyphs.append(glyph)

    return font


def write(font: Font) -> bytes:
    count = len(font.glyphs)
    if count <= 0:
        raise FontError("a font needs at least one glyph")

    info_offset = HEADER_SIZE
    offset_block = info_offset + INFO_SIZE
    width_block = offset_block + count * 2
    height_block = width_block + count
    data_block = height_block + count * 2

    bodies = []
    offsets = []
    position = 0
    for glyph in font.glyphs:
        offsets.append(position)
        body = _pack_rows(glyph.pixels, glyph.width, glyph.height) if glyph.pixels else b""
        bodies.append(body)
        position += len(body)

    total = data_block + position
    if total > 0xFFFF:
        raise FontError(f"the font would be {total} bytes, which its header cannot describe")

    out = bytearray(total)
    struct.pack_into("<HBBHHHHH", out, 0, total, COMPRESS_NEW, 2, info_offset,
                     offset_block, width_block, data_block, height_block)

    info = bytearray(INFO_SIZE)
    info[INFO_MAX_HEIGHT] = font.max_height
    info[INFO_MAX_WIDTH] = font.max_width
    out[info_offset:info_offset + INFO_SIZE] = info

    for index, glyph in enumerate(font.glyphs):
        struct.pack_into("<H", out, offset_block + index * 2, offsets[index])
        out[width_block + index] = glyph.width
        struct.pack_into("<H", out, height_block + index * 2,
                         ((glyph.height & 0xFF) << 8) | (glyph.first_row & 0xFF))

    position = data_block
    for body in bodies:
        out[position:position + len(body)] = body
        position += len(body)

    return bytes(out)


def magnify(font: Font, factor: int) -> Font:
    """Grows every glyph pixel to a square of the factor.

    The pipeline replaces the glyph pixels with ones upscaled by a pixel-art
    method and keeps these metrics, which this function establishes.
    """
    if factor < 1:
        raise FontError("the factor must be at least one")
    if factor == 1:
        return font

    grown = Font(font.max_width * factor, font.max_height * factor)

    for glyph in font.glyphs:
        big = Glyph(glyph.width * factor, glyph.first_row * factor, glyph.height * factor)
        if glyph.pixels:
            rows = bytearray()
            for y in range(glyph.height):
                row = glyph.pixels[y * glyph.width:(y + 1) * glyph.width]
                wide = bytes(value for value in row for _ in range(factor))
                rows += wide * factor
            big.pixels = bytes(rows)
        grown.glyphs.append(big)

    return grown
