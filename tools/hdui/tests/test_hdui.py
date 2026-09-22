"""Tests for the interface artwork pipeline.

Every input is made here: the shapes, the fonts, the palette and the archive are
all synthesised, so nothing in this file needs the retail data. What it cannot
cover is the upscaler itself, which is a separate program and a GPU.
"""

from pathlib import Path
from tempfile import TemporaryDirectory
import struct
import sys
import unittest

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

import build_hdui_pack
import fnt
import fnt2png
import mixextract
import mixreader
import palette as palette_module
import png
import png2fnt
import png2shp
import shp
import shp2png
import upscale_ui


def make_palette() -> palette_module.Palette:
    """A palette whose colours are far enough apart to match unambiguously."""
    colors = [(0, 0, 0)]
    for index in range(1, 256):
        colors.append(((index * 7) % 256, (index * 29) % 256, (index * 53) % 256))
    return palette_module.Palette(colors)


def make_shape(frames: int = 2, width: int = 6, height: int = 4, rle: bool = True) -> shp.Shape:
    shape = shp.Shape(width + 2, height + 2, 0)
    for index in range(frames):
        pixels = bytearray(width * height)
        for position in range(width * height):
            # A mix of transparent and solid pixels, so both run lengths and
            # literals appear in an encoded row.
            pixels[position] = 0 if (position + index) % 3 == 0 else 1 + ((position + index) % 20)
        flags = shp.FLAG_TRANSPARENT | (shp.FLAG_RLE if rle else 0)
        shape.frames.append(shp.Frame(index, index + 1, width, height, flags,
                                      (index + 1, index + 2, index + 3), bytes(pixels)))
    return shape


# The shipped fonts draw with indices spread across the sixteen rather than the lowest few:
# one uses 0 to 3, another 0, 1 and 11, another 0, 2, 3 and 11 to 15.
FONT_LEVELS = (0, 1, 2, 3, 11, 12, 15)


def make_font(glyphs: int = 20) -> fnt.Font:
    font = fnt.Font(max_width=6, max_height=8)
    for index in range(glyphs):
        width = 3 + index % 4
        height = 2 + index % 3
        pixels = bytes(FONT_LEVELS[(x + y + index) % len(FONT_LEVELS)] if (x + y) % 2 == 0 else 0
                       for y in range(height) for x in range(width))
        font.glyphs.append(fnt.Glyph(width, index % 3, height, pixels))
    return font


class ShapeCodec(unittest.TestCase):
    def test_round_trip_keeps_every_field(self):
        for rle in (True, False):
            with self.subTest(rle=rle):
                original = make_shape(rle=rle)
                again = shp.read(shp.write(original))

                self.assertEqual(again.width, original.width)
                self.assertEqual(again.height, original.height)
                self.assertEqual(len(again.frames), len(original.frames))
                for before, after in zip(original.frames, again.frames):
                    self.assertEqual((after.x, after.y, after.width, after.height, after.flags),
                                     (before.x, before.y, before.width, before.height, before.flags))
                    self.assertEqual(after.color, before.color)
                    self.assertEqual(after.pixels, before.pixels)

    def test_an_empty_frame_stays_empty(self):
        original = make_shape(frames=1)
        original.frames.append(shp.Frame(0, 0, 4, 4, 0, (0, 0, 0), b""))
        again = shp.read(shp.write(original))
        self.assertEqual(again.frames[1].pixels, b"")

    def test_a_frame_past_the_signed_bound_round_trips(self):
        # An enlarged sidebar backdrop is the case: opaque, so RLE buys nothing, and four
        # times the pixels of the artwork it came from.
        original = shp.Shape(width=336, height=102, flags=0)
        pixels = bytes((index % 255) + 1 for index in range(336 * 102))
        original.frames.append(shp.Frame(0, 0, 336, 102, 0, (1, 2, 3), pixels))

        again = shp.read(shp.write(original))

        self.assertEqual(len(pixels), 34272)
        self.assertEqual(again.frames[0].pixels, pixels)

    def test_magnify_doubles_offsets_and_pixels(self):
        original = make_shape(frames=1, width=3, height=2)
        grown = shp.magnify(original, 2)

        self.assertEqual((grown.width, grown.height), (original.width * 2, original.height * 2))
        frame, big = original.frames[0], grown.frames[0]
        self.assertEqual((big.x, big.y), (frame.x * 2, frame.y * 2))
        self.assertEqual((big.width, big.height), (frame.width * 2, frame.height * 2))
        for y in range(big.height):
            for x in range(big.width):
                self.assertEqual(big.pixels[y * big.width + x],
                                 frame.pixels[(y // 2) * frame.width + (x // 2)])

    def test_a_frame_too_large_to_describe_records_zero(self):
        huge = shp.Shape(400, 400, 0)
        huge.frames.append(shp.Frame(0, 0, 400, 400, 0, (0, 0, 0), bytes(400 * 400)))

        written = shp.write(huge)
        size = struct.unpack_from("<H", written, shp.HEADER_SIZE + 10)[0]
        offset = struct.unpack_from("<i", written, shp.HEADER_SIZE + shp.DATA_OFFSET)[0]

        self.assertEqual(size, 0)
        self.assertNotEqual(offset, 0)
        self.assertEqual(shp.read(written).frames[0].pixels, huge.frames[0].pixels)


class FontCodec(unittest.TestCase):
    def test_round_trip_keeps_every_glyph(self):
        original = make_font()
        again = fnt.read(fnt.write(original))

        self.assertEqual((again.max_width, again.max_height),
                         (original.max_width, original.max_height))
        self.assertEqual(len(again.glyphs), len(original.glyphs))
        for before, after in zip(original.glyphs, again.glyphs):
            self.assertEqual((after.width, after.first_row, after.height),
                             (before.width, before.first_row, before.height))
            self.assertEqual(after.pixels, before.pixels)

    def test_the_header_describes_the_file_it_wrote(self):
        data = fnt.write(make_font())
        length, _compress, _blocks, info, offsets, widths, _data, heights = \
            struct.unpack_from("<HBBHHHHH", data, 0)

        self.assertEqual(length, len(data))
        self.assertLess(info, offsets)
        self.assertLess(offsets, widths)
        self.assertLess(widths, heights)

    def test_the_header_states_the_packing_the_pixels_use(self):
        """The engine branches on the compression byte to decide how to read a row, so a
        font labelled one way while packed the other draws as garbage."""
        for compress, per_row in ((0, lambda w: (w + 1) // 2), (fnt.COMPRESS_NEW, lambda w: w)):
            with self.subTest(compress=compress):
                font = fnt.Font(max_width=6, max_height=4, compress=compress)
                font.glyphs.append(fnt.Glyph(5, 1, 2, bytes([1, 2, 3, 4, 5, 6, 7, 8, 9, 10])))

                data = fnt.write(font)

                self.assertEqual(data[2], compress)
                data_block = struct.unpack_from("<H", data, 10)[0]
                self.assertEqual(len(data) - data_block, per_row(5) * 2)

                again = fnt.read(data)
                self.assertEqual(again.compress, compress)
                self.assertEqual(again.glyphs[0].pixels, font.glyphs[0].pixels)

    def test_magnify_doubles_the_metrics(self):
        original = make_font(glyphs=4)
        grown = fnt.magnify(original, 2)

        self.assertEqual(grown.max_height, original.max_height * 2)
        for before, after in zip(original.glyphs, grown.glyphs):
            self.assertEqual(after.width, before.width * 2)
            self.assertEqual(after.first_row, before.first_row * 2)
            self.assertEqual(after.height, before.height * 2)


class PngCodec(unittest.TestCase):
    def test_round_trip_for_every_mode_used(self):
        for mode in (png.GREY, png.RGB, png.RGBA):
            with self.subTest(mode=mode):
                image = png.new(5, 3, mode)
                for index in range(len(image.pixels)):
                    image.pixels[index] = (index * 37) % 256
                again = png.read(png.write(image))

                self.assertEqual((again.width, again.height, again.mode), (5, 3, mode))
                self.assertEqual(bytes(again.pixels), bytes(image.pixels))

    def test_a_filtered_image_decodes(self):
        # Filters are what an outside upscaler will write; build one by hand.
        import zlib
        width, height = 4, 2
        raw = bytearray()
        for y in range(height):
            raw.append(1)                       # Sub: each byte is a delta from its left.
            raw += bytes((1, 2, 3, 4))
        body = zlib.compress(bytes(raw), 9)

        def chunk(kind, payload):
            return (struct.pack(">I", len(payload)) + kind + payload
                    + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF))

        data = (png.SIGNATURE
                + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0))
                + chunk(b"IDAT", body) + chunk(b"IEND", b""))

        image = png.read(data)
        self.assertEqual(bytes(image.row(0)), bytes((1, 3, 6, 10)))


class Palette(unittest.TestCase):
    def test_nearest_finds_the_colour_itself(self):
        pal = make_palette()
        for index in (1, 17, 200, 255):
            self.assertEqual(pal.nearest(pal.colors[index]), index)

    def test_nearest_searches_only_the_indices_given(self):
        pal = make_palette()
        allowed = (10, 20, 30)
        self.assertIn(pal.nearest(pal.colors[200], allowed), allowed)

    def test_a_palette_round_trips_through_a_file(self):
        pal = make_palette()
        with TemporaryDirectory() as scratch:
            path = Path(scratch) / "test.pal"
            pal.save(path)
            again = palette_module.Palette.load(path)
        # Six bits per channel, so the widened values come back on the same step.
        self.assertEqual(again.colors[5], tuple(part // 4 * 4 for part in pal.colors[5]))


class ShapeSheets(unittest.TestCase):
    def test_a_shape_survives_the_whole_pipeline_at_2x(self):
        pal = make_palette()
        original = make_shape(frames=3)

        with TemporaryDirectory() as scratch:
            work = Path(scratch)
            source = work / "TEST.SHP"
            source.write_bytes(shp.write(original))

            folder = work / "frames"
            shp2png.export(source, folder, pal)
            self.assertTrue((folder / "shape.json").exists())
            self.assertTrue((folder / "frame0000.mask.png").exists())

            for frame in sorted(folder.glob("frame*.png")):
                image = png.read(frame.read_bytes())
                frame.write_bytes(png.write(upscale_ui.repeat(image, 2)))

            built = png2shp.build(folder, pal, 2)

        self.assertEqual(built.width, original.width * 2)
        self.assertEqual(len(built.frames), len(original.frames))
        for before, after in zip(original.frames, built.frames):
            self.assertEqual((after.x, after.y), (before.x * 2, before.y * 2))
            self.assertEqual((after.width, after.height), (before.width * 2, before.height * 2))
            self.assertEqual(after.color, before.color)
            for y in range(after.height):
                for x in range(after.width):
                    self.assertEqual(after.pixels[y * after.width + x],
                                     before.pixels[(y // 2) * before.width + (x // 2)])

    def test_the_encoder_refuses_frames_of_the_wrong_size(self):
        pal = make_palette()
        with TemporaryDirectory() as scratch:
            work = Path(scratch)
            source = work / "TEST.SHP"
            source.write_bytes(shp.write(make_shape(frames=1)))
            folder = work / "frames"
            shp2png.export(source, folder, pal)

            with self.assertRaises(png2shp.EncodeError):
                png2shp.build(folder, pal, 2)       # The frames were never enlarged.


class FontSheets(unittest.TestCase):
    def test_a_font_survives_the_whole_pipeline_at_2x(self):
        original = make_font(glyphs=18)

        with TemporaryDirectory() as scratch:
            work = Path(scratch)
            source = work / "TEST.FNT"
            source.write_bytes(fnt.write(original))

            folder = work / "sheet"
            fnt2png.export(source, folder)
            sheet = folder / "sheet.png"
            image = png.read(sheet.read_bytes())
            sheet.write_bytes(png.write(upscale_ui.repeat(image, 2)))

            built = png2fnt.build(folder, 2)

        self.assertEqual(built.max_height, original.max_height * 2)
        self.assertEqual(len(built.glyphs), len(original.glyphs))
        for before, after in zip(original.glyphs, built.glyphs):
            self.assertEqual(after.width, before.width * 2)
            self.assertEqual(after.height, before.height * 2)
            for y in range(after.height):
                for x in range(after.width):
                    self.assertEqual(after.pixels[y * after.width + x],
                                     before.pixels[(y // 2) * before.width + (x // 2)])

    def test_an_enlarged_font_is_packed_to_fit_its_offsets(self):
        """A glyph offset is sixteen bits, so a font that states a byte per pixel is still
        built two to a byte once enlarged; the wider form outgrows the field."""
        original = make_font(glyphs=64)
        original.compress = fnt.COMPRESS_NEW

        with TemporaryDirectory() as scratch:
            work = Path(scratch)
            source = work / "TEST.FNT"
            source.write_bytes(fnt.write(original))

            folder = work / "sheet"
            fnt2png.export(source, folder)
            sheet = folder / "sheet.png"
            sheet.write_bytes(png.write(upscale_ui.repeat(png.read(sheet.read_bytes()), 2)))

            built = png2fnt.build(folder, 2)
            data = fnt.write(built)

        self.assertNotEqual(built.compress, fnt.COMPRESS_NEW)

        wanted = fnt.magnify(original, 2)
        again = fnt.read(data)
        for expected, after in zip(wanted.glyphs, again.glyphs):
            self.assertEqual((after.width, after.height), (expected.width, expected.height))
            self.assertEqual(after.pixels, expected.pixels)

    def test_the_indices_a_font_draws_with_survive(self):
        original = make_font(glyphs=32)
        used = sorted({value for glyph in original.glyphs for value in glyph.pixels})

        with TemporaryDirectory() as scratch:
            work = Path(scratch)
            source = work / "TEST.FNT"
            source.write_bytes(fnt.write(original))

            folder = work / "sheet"
            fnt2png.export(source, folder)
            sheet = folder / "sheet.png"
            sheet.write_bytes(png.write(upscale_ui.repeat(png.read(sheet.read_bytes()), 2)))

            built = png2fnt.build(folder, 2)

        self.assertIn(15, used)
        self.assertEqual(sorted({value for glyph in built.glyphs for value in glyph.pixels}), used)

    def test_the_written_font_reads_back_as_a_font(self):
        original = make_font(glyphs=8)
        with TemporaryDirectory() as scratch:
            folder = Path(scratch) / "sheet"
            source = Path(scratch) / "TEST.FNT"
            source.write_bytes(fnt.write(original))
            fnt2png.export(source, folder)
            sheet = folder / "sheet.png"
            sheet.write_bytes(png.write(upscale_ui.repeat(png.read(sheet.read_bytes()), 2)))
            data = fnt.write(png2fnt.build(folder, 2))

        again = fnt.read(data)
        self.assertEqual(again.max_width, original.max_width * 2)


class Extraction(unittest.TestCase):
    def test_named_members_come_out_of_an_archive(self):
        members = {"SIDE1.SHP": shp.write(make_shape()), "TABS.SHP": shp.write(make_shape(1))}

        with TemporaryDirectory() as scratch:
            archive = Path(scratch) / "TEST.MIX"
            mixreader.write_mix(archive, members)

            out = Path(scratch) / "raw"
            written = mixextract.extract([archive], list(members) + ["ABSENT.SHP"], out)

        self.assertEqual(set(written), set(members))
        self.assertEqual(sorted(p.name for p in written.values()), ["SIDE1.SHP", "TABS.SHP"])


class PackBuild(unittest.TestCase):
    def test_a_pack_is_built_from_an_archive_without_a_gpu(self):
        pal = make_palette()
        members = {name: shp.write(make_shape(frames=2)) for name in build_hdui_pack.SHAPES}
        members.update({name: fnt.write(make_font(glyphs=10)) for name in build_hdui_pack.FONTS})

        with TemporaryDirectory() as scratch:
            work = Path(scratch)
            pal.save(work / "SIDEBAR.PAL")
            members["SIDEBAR.PAL"] = (work / "SIDEBAR.PAL").read_bytes()

            archive = work / "TEST.MIX"
            mixreader.write_mix(archive, members)

            out = work / "HD"
            built = build_hdui_pack.build([archive], out, scale=2, upscale=False)

            self.assertEqual(built["missing"], [])
            self.assertEqual(len(built["shapes"]), len(build_hdui_pack.SHAPES))
            self.assertEqual(len(built["fonts"]), len(build_hdui_pack.FONTS))

            manifest = (out / "HDPACK.INI").read_text()
            self.assertIn("[UI]\nScale=2", manifest)
            self.assertIn("SIDE1.SHP=2", manifest)

            side = shp.read((out / "SIDE1.SHP").read_bytes())
            self.assertEqual(side.width, shp.read(members["SIDE1.SHP"]).width * 2)

            font = fnt.read((out / "8POINT.FNT").read_bytes())
            self.assertEqual(font.max_height, fnt.read(members["8POINT.FNT"]).max_height * 2)

    def test_a_destination_inside_run_is_refused(self):
        with TemporaryDirectory() as scratch:
            with self.assertRaises(build_hdui_pack.BuildError):
                build_hdui_pack.build([], Path(scratch) / "Run" / "HD")


if __name__ == "__main__":
    unittest.main()
