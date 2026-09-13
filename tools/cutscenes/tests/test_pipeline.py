"""Tests for the cutscene pipeline.

Every input is synthesised here. The stages are exercised on a short generated
clip that stands in for a VQA: the only thing it cannot cover is the VQA decode
itself, which is ffmpeg's, not this tool's.
"""

from pathlib import Path
from tempfile import TemporaryDirectory
import json
import shutil
import struct
import sys
import unittest
import zlib


TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

import common
import dump_frames
import encode as encode_stage
import extract_movies
import mixreader
import pipeline
import upscale as upscale_stage


SOURCE_WIDTH = 64
SOURCE_HEIGHT = 40
SOURCE_FRAMES = 16
SOURCE_FPS = 15


def have_ffmpeg() -> bool:
    try:
        return common.ffmpeg(required=False) is not None
    except common.PipelineError:
        return False


def make_config(root: Path, **overrides) -> common.Config:
    data = json.loads((TOOLS / "config.json").read_text(encoding="utf-8"))
    data["work"] = str(root / "work")
    data["out"] = str(root / "out")
    data["target_width"] = 256
    data["target_height"] = 160
    data["upscale_factor"] = 2
    data["preset"] = "ultrafast"
    data["preview_frames"] = 4
    data.update(overrides)
    path = root / "config.json"
    path.write_text(json.dumps(data), encoding="utf-8")
    return common.Config.load(path)


def make_clip(path: Path, frames: int = SOURCE_FRAMES, audio: bool = True) -> Path:
    """Encodes a short clip standing in for an extracted VQA."""
    args = [
        "-f", "lavfi", "-i",
        f"testsrc2=size={SOURCE_WIDTH}x{SOURCE_HEIGHT}:rate={SOURCE_FPS}",
    ]
    if audio:
        args += ["-f", "lavfi", "-i", "sine=frequency=440:sample_rate=22050"]
    args += ["-frames:v", str(frames), "-c:v", "libx264", "-preset", "ultrafast",
             "-pix_fmt", "yuv420p"]
    if audio:
        args += ["-c:a", "aac", "-ac", "1", "-ar", "22050", "-shortest"]
    args += [str(path)]
    common.run_ffmpeg(args)
    return path


class NameChecksumTests(unittest.TestCase):
    """The index checksum has to match what the engine computes for a name."""

    @staticmethod
    def staged(name: str) -> int:
        """Recomputes the checksum the long way, as CRCEngine accumulates it."""
        data = name.upper().encode("ascii")
        crc = 0
        buffer = bytearray()
        for byte in data:
            buffer.append(byte)
            if len(buffer) == 4:
                crc = zlib.crc32(bytes(buffer), crc)
                buffer.clear()
        if buffer:
            index = len(buffer)
            first = buffer[0]
            buffer.append(index)
            while len(buffer) < 4:
                buffer.append(first)
            crc = zlib.crc32(bytes(buffer), crc)
        return crc - (1 << 32) if crc >= (1 << 31) else crc

    def test_matches_the_staged_accumulation(self):
        for name in ("GDI1.VQA", "A", "AB", "ABC", "ABCD", "ABCDE", "NOD_M02.VQA"):
            self.assertEqual(mixreader.name_key(name), self.staged(name), name)

    def test_is_case_insensitive(self):
        self.assertEqual(mixreader.name_key("gdi1.vqa"), mixreader.name_key("GDI1.VQA"))

    def test_fits_a_signed_int(self):
        for name in ("GDI_M09A.VQA", "FSNODFNL.VQA", "X"):
            self.assertGreaterEqual(mixreader.name_key(name), -(1 << 31))
            self.assertLess(mixreader.name_key(name), 1 << 31)


class MixFileTests(unittest.TestCase):
    def setUp(self):
        self.temp = TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.addCleanup(self.temp.cleanup)
        self.members = {
            "GDI1.VQA": b"first movie payload",
            "NOD_M02.VQA": b"second" * 10,
            "EVA.VQA": b"",
        }

    def test_reads_back_a_plain_archive(self):
        path = mixreader.write_mix(self.root / "MOVIES01.MIX", self.members)
        with mixreader.MixFile(path) as archive:
            self.assertEqual(archive.count, len(self.members))
            for name, data in self.members.items():
                self.assertEqual(archive.read(name), data, name)

    def test_reads_back_an_extended_archive(self):
        path = mixreader.write_mix(self.root / "MOVIES02.MIX", self.members,
                                   extended=True)
        with mixreader.MixFile(path) as archive:
            self.assertEqual(archive.read("GDI1.VQA"), self.members["GDI1.VQA"])

    def test_index_is_sorted_by_checksum(self):
        path = mixreader.write_mix(self.root / "S.MIX", self.members)
        with mixreader.MixFile(path) as archive:
            keys = [member.key for member in archive.members]
            self.assertEqual(keys, sorted(keys))

    def test_refuses_an_encrypted_index(self):
        path = self.root / "E.MIX"
        path.write_bytes(struct.pack("<hh", 0, mixreader.FLAG_ENCRYPTED) + b"\0" * 64)
        with self.assertRaises(mixreader.MixError) as caught:
            mixreader.MixFile(path)
        self.assertIn("XCC", str(caught.exception))

    def test_reports_a_truncated_index(self):
        path = mixreader.write_mix(self.root / "T.MIX", self.members)
        data = path.read_bytes()
        path.write_bytes(data[:10])
        with self.assertRaises(mixreader.MixError):
            mixreader.MixFile(path)

    def test_reports_a_file_too_short_for_a_header(self):
        path = self.root / "Z.MIX"
        path.write_bytes(b"\x01")
        with self.assertRaises(mixreader.MixError):
            mixreader.MixFile(path)

    def test_resolve_reports_only_the_names_held(self):
        path = mixreader.write_mix(self.root / "R.MIX", self.members)
        with mixreader.MixFile(path) as archive:
            found = archive.resolve(["GDI1.VQA", "ABSENT.VQA"])
        self.assertEqual(sorted(found), ["GDI1.VQA"])


class ExtractionTests(unittest.TestCase):
    def setUp(self):
        self.temp = TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.addCleanup(self.temp.cleanup)
        self.config = make_config(self.root)
        self.data = self.root / "data"
        self.data.mkdir()
        mixreader.write_mix(self.data / "MOVIES01.MIX", {"GDI1.VQA": b"one" * 8})
        mixreader.write_mix(self.data / "MOVIES02.MIX",
                            {"NOD1.VQA": b"two" * 8, "GDI1.VQA": b"shadowed"})

    def test_extracts_from_both_archives(self):
        found = extract_movies.extract(self.config, self.data, ["GDI1", "NOD1", "GONE"])
        self.assertEqual(sorted(found), ["GDI1", "NOD1"])
        self.assertEqual((self.config.work / "vqa" / "GDI1.VQA").read_bytes(), b"one" * 8)

    def test_the_first_archive_named_wins(self):
        found = extract_movies.extract(self.config, self.data, ["GDI1"])
        self.assertEqual(found["GDI1"].read_bytes(), b"one" * 8)

    def test_reports_a_data_directory_with_no_archives(self):
        empty = self.root / "empty"
        empty.mkdir()
        with self.assertRaises(common.PipelineError):
            extract_movies.extract(self.config, empty, ["GDI1"])


class MovieInventoryTests(unittest.TestCase):
    def test_reads_the_enumerator_names(self):
        names = common.movie_names()
        self.assertIn("GDI_M02", names)
        self.assertIn("FSNODFNL", names)
        for sentinel in ("NONE", "COUNT", "FIRST"):
            self.assertNotIn(sentinel, names)

    def test_rules_ini_wins_when_it_is_there(self):
        with TemporaryDirectory() as temp:
            rules = Path(temp) / "rules.ini"
            rules.write_text("[General]\nX=1\n\n[Movies]\n1=GDI1\n2=NOD1 ; trailing\n"
                             "\n[After]\n3=IGNORED\n", encoding="utf-8")
            self.assertEqual(common.movie_names(rules), ["GDI1", "NOD1"])

    def test_falls_back_when_rules_ini_is_absent(self):
        self.assertEqual(common.movie_names(Path("/nonexistent/rules.ini")),
                         common.movie_names())


class SafetyTests(unittest.TestCase):
    def test_refuses_to_write_into_the_retail_directory(self):
        with self.assertRaises(common.PipelineError) as caught:
            common.guard_output(common.RUN_DIR / "MOVIES01.MIX")
        self.assertIn("retail", str(caught.exception))

    def test_allows_a_destination_outside_it(self):
        with TemporaryDirectory() as temp:
            self.assertTrue(common.ensure_dir(Path(temp) / "work" / "vqa").is_dir())

    def test_missing_tool_names_its_environment_variable(self):
        with self.assertRaises(common.PipelineError) as caught:
            common.find_tool("opents-no-such-tool", "OPENTS_NO_SUCH_TOOL")
        self.assertIn("OPENTS_NO_SUCH_TOOL", str(caught.exception))


class FrameHelperTests(unittest.TestCase):
    def test_orders_frames_numerically_and_ignores_other_files(self):
        with TemporaryDirectory() as temp:
            directory = Path(temp)
            for stem in ("00010", "00002", "00001"):
                (directory / f"{stem}.png").write_bytes(b"")
            (directory / "notes.txt").write_bytes(b"")
            (directory / "thumb.png").write_bytes(b"")
            names = [p.name for p in common.frame_files(directory)]
            self.assertEqual(names, ["00001.png", "00002.png", "00010.png"])

    def test_pattern_follows_the_suffix_in_use(self):
        with TemporaryDirectory() as temp:
            directory = Path(temp)
            (directory / "00001.jpg").write_bytes(b"")
            self.assertTrue(common.frame_pattern(directory).endswith("%05d.jpg"))


@unittest.skipUnless(have_ffmpeg(), "ffmpeg is not installed")
class StageTests(unittest.TestCase):
    """Runs stages 2 to 4 on a generated clip standing in for a VQA."""

    @classmethod
    def setUpClass(cls):
        cls.temp = TemporaryDirectory()
        cls.clip = Path(cls.temp.name) / "SOURCE.mp4"
        make_clip(cls.clip)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def setUp(self):
        self.temp = TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.addCleanup(self.temp.cleanup)
        self.config = make_config(self.root)

    def test_probe_reports_the_clip(self):
        media = common.probe(self.clip)
        self.assertEqual((media.width, media.height), (SOURCE_WIDTH, SOURCE_HEIGHT))
        self.assertEqual(media.frames, SOURCE_FRAMES)
        self.assertAlmostEqual(media.fps, SOURCE_FPS, places=3)
        self.assertTrue(media.has_audio)

    def test_dump_writes_one_png_per_frame_and_a_wav(self):
        media, frames, audio = dump_frames.dump(self.config, "PROOF", self.clip)
        self.assertEqual(len(common.frame_files(frames)), SOURCE_FRAMES)
        self.assertEqual(media.frames, SOURCE_FRAMES)
        self.assertIsNotNone(audio)
        self.assertTrue(audio.is_file())

    def test_dump_rewrites_the_sound_at_the_configured_rate(self):
        _media, _frames, audio = dump_frames.dump(self.config, "PROOF", self.clip)
        wav = audio.read_bytes()
        channels, rate = struct.unpack_from("<HI", wav, 22)
        self.assertEqual(rate, self.config.audio_rate)
        self.assertEqual(channels, self.config.audio_channels)

    def test_dump_keeps_a_silent_source_going(self):
        silent = self.root / "SILENT.mp4"
        make_clip(silent, audio=False)
        media, frames, audio = dump_frames.dump(self.config, "SILENT", silent)
        self.assertFalse(media.has_audio)
        self.assertIsNone(audio)
        self.assertEqual(len(common.frame_files(frames)), SOURCE_FRAMES)

    def test_dump_reuses_frames_already_present(self):
        dump_frames.dump(self.config, "PROOF", self.clip)
        first = common.frame_files(self.config.frames_dir("PROOF"))[0]
        stamp = first.stat().st_mtime_ns
        dump_frames.dump(self.config, "PROOF", self.clip)
        self.assertEqual(first.stat().st_mtime_ns, stamp)

    def test_upscale_enlarges_every_frame(self):
        dump_frames.dump(self.config, "PROOF", self.clip)
        target, ran, rate = upscale_stage.upscale(self.config, "PROOF", backend="lanczos")
        self.assertEqual(ran, SOURCE_FRAMES)
        self.assertGreater(rate, 0)
        media = common.probe(common.frame_files(target)[0])
        self.assertEqual((media.width, media.height),
                         (SOURCE_WIDTH * self.config.upscale_factor,
                          SOURCE_HEIGHT * self.config.upscale_factor))

    def test_upscale_resumes_after_an_interruption(self):
        dump_frames.dump(self.config, "PROOF", self.clip)
        upscale_stage.upscale(self.config, "PROOF", backend="lanczos")
        target = self.config.upscaled_dir("PROOF")
        survivors = common.frame_files(target)
        for frame in survivors[SOURCE_FRAMES // 2:]:
            frame.unlink()
        _target, ran, _rate = upscale_stage.upscale(self.config, "PROOF",
                                                    backend="lanczos")
        self.assertEqual(ran, SOURCE_FRAMES - SOURCE_FRAMES // 2)
        self.assertEqual(len(common.frame_files(target)), SOURCE_FRAMES)

    def test_upscale_does_no_work_when_everything_is_present(self):
        dump_frames.dump(self.config, "PROOF", self.clip)
        upscale_stage.upscale(self.config, "PROOF", backend="lanczos")
        _target, ran, _rate = upscale_stage.upscale(self.config, "PROOF",
                                                    backend="lanczos")
        self.assertEqual(ran, 0)

    def test_denoise_produces_the_same_frame_count(self):
        dump_frames.dump(self.config, "PROOF", self.clip)
        target = upscale_stage.denoise(self.config, "PROOF")
        self.assertEqual(len(common.frame_files(target)), SOURCE_FRAMES)

    def test_upscale_can_write_jpg_intermediates(self):
        dump_frames.dump(self.config, "PROOF", self.clip)
        target, _ran, _rate = upscale_stage.upscale(self.config, "PROOF",
                                                    backend="lanczos", fmt="jpg")
        self.assertTrue(all(p.suffix == ".jpg" for p in common.frame_files(target)))

    def test_upscale_reports_an_unknown_backend(self):
        dump_frames.dump(self.config, "PROOF", self.clip)
        with self.assertRaises(common.PipelineError):
            upscale_stage.upscale(self.config, "PROOF", backend="nonesuch")

    def test_encode_delivers_the_configured_size(self):
        _media, _frames, audio = dump_frames.dump(self.config, "PROOF", self.clip)
        upscale_stage.upscale(self.config, "PROOF", backend="lanczos")
        output = encode_stage.encode(self.config, "PROOF", fps=SOURCE_FPS, audio=audio)
        media = common.probe(output)
        self.assertEqual((media.width, media.height),
                         (self.config.target_width, self.config.target_height))
        self.assertEqual(media.frames, SOURCE_FRAMES)
        self.assertTrue(media.has_audio)

    def test_preview_stops_at_the_configured_frame_count(self):
        dump_frames.dump(self.config, "PROOF", self.clip)
        upscale_stage.upscale(self.config, "PROOF", backend="lanczos")
        output = encode_stage.encode(self.config, "PROOF", fps=SOURCE_FPS, preview=True)
        self.assertTrue(output.name.endswith("-preview.mp4"))
        self.assertEqual(common.probe(output).frames, self.config.preview_frames)

    def test_encode_reports_a_missing_upscale(self):
        with self.assertRaises(common.PipelineError):
            encode_stage.encode(self.config, "NOTHING")


@unittest.skipUnless(have_ffmpeg(), "ffmpeg is not installed")
class DriverTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = TemporaryDirectory()
        cls.clip = Path(cls.temp.name) / "SOURCE.mp4"
        make_clip(cls.clip)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def setUp(self):
        self.temp = TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.addCleanup(self.temp.cleanup)
        self.config = make_config(self.root)

    def extracted(self, name: str) -> Path:
        target = common.ensure_dir(self.config.work / "vqa") / f"{name}.VQA"
        shutil.copy2(self.clip, target)
        return target

    def test_proof_records_a_manifest_entry(self):
        entry = pipeline.run_one(self.config, "PROOF", source=self.clip,
                                 backend="lanczos")
        self.assertEqual(entry["status"], "ok")
        self.assertEqual(entry["source"]["frames"], SOURCE_FRAMES)
        self.assertEqual(entry["frames_upscaled"], SOURCE_FRAMES)
        self.assertEqual(len(entry["output_md5"]), 32)
        self.assertGreater(entry["output_bytes"], 0)
        self.assertEqual(entry["output_md5"],
                         common.md5_of(self.config.output_file("PROOF")))

    def test_batch_runs_what_was_extracted_and_survives_a_failure(self):
        self.extracted("GOOD")
        broken = common.ensure_dir(self.config.work / "vqa") / "BROKEN.VQA"
        broken.write_bytes(b"not a movie")
        args = _Args(backend="lanczos", denoise=False, fmt="png", rife=False,
                     force=False, delete_frames=False, movies=[])
        self.assertEqual(pipeline.command_batch(self.config, args), 1)
        manifest = common.load_manifest(self.config)
        self.assertEqual(manifest["movies"]["GOOD"]["status"], "ok")
        self.assertEqual(manifest["movies"]["BROKEN"]["status"], "failed")
        self.assertTrue(self.config.output_file("GOOD").is_file())

    def test_batch_can_drop_frames_once_a_movie_is_encoded(self):
        self.extracted("GOOD")
        args = _Args(backend="lanczos", denoise=False, fmt="png", rife=False,
                     force=False, delete_frames=True, movies=["GOOD"])
        pipeline.command_batch(self.config, args)
        self.assertFalse(self.config.frames_dir("GOOD").is_dir())
        self.assertFalse(self.config.upscaled_dir("GOOD").is_dir())
        self.assertTrue(self.config.output_file("GOOD").is_file())

    def test_batch_skips_a_movie_already_encoded(self):
        self.extracted("GOOD")
        args = _Args(backend="lanczos", denoise=False, fmt="png", rife=False,
                     force=False, delete_frames=False, movies=["GOOD"])
        pipeline.command_batch(self.config, args)
        stamp = self.config.output_file("GOOD").stat().st_mtime_ns
        pipeline.command_batch(self.config, args)
        self.assertEqual(self.config.output_file("GOOD").stat().st_mtime_ns, stamp)

    def test_compare_writes_three_stills(self):
        dump_frames.dump(self.config, "PROOF", self.clip)
        args = _Args(movie="PROOF", frame=4, backend="lanczos")
        self.assertEqual(pipeline.command_compare(self.config, args), 0)
        produced = sorted(p.name for p in self.config.compare_dir().iterdir())
        self.assertEqual(produced, [
            "PROOF-00004-original.png",
            "PROOF-00004-x4-denoise.png",
            "PROOF-00004-x4.png",
        ])

    def test_compare_rejects_a_frame_outside_the_movie(self):
        dump_frames.dump(self.config, "PROOF", self.clip)
        args = _Args(movie="PROOF", frame=SOURCE_FRAMES, backend="lanczos")
        with self.assertRaises(common.PipelineError):
            pipeline.command_compare(self.config, args)

    def test_verify_separates_optional_from_missing(self):
        rules = self.root / "rules.ini"
        rules.write_text("[Movies]\n1=GDI1\n2=WWLOGO\n", encoding="utf-8")
        args = _Args(rules=rules)
        self.assertEqual(pipeline.command_verify(self.config, args), 1)
        common.ensure_dir(self.config.out)
        self.config.output_file("GDI1").write_bytes(b"")
        self.assertEqual(pipeline.command_verify(self.config, args), 0)


class _Args:
    def __init__(self, **fields):
        self.__dict__.update(fields)


if __name__ == "__main__":
    unittest.main()
