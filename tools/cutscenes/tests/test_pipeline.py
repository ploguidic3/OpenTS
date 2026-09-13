"""Tests for the cutscene pipeline.

Every input is synthesised here. The stages are exercised on a short generated
clip that stands in for a VQA: the only thing it cannot cover is the VQA decode
itself, which is ffmpeg's, not this tool's.
"""

from pathlib import Path
from tempfile import TemporaryDirectory
import json
import os
import shutil
import struct
import sys
import unittest
import unittest.mock
import zlib


TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

import blowfish
import common
import dump_frames
import encode as encode_stage
import extract_movies
import mixcrypt
import mixreader
import pipeline
import upscale as upscale_stage


SOURCE_WIDTH = 64
SOURCE_HEIGHT = 40
SOURCE_FRAMES = 16
SOURCE_FPS = 15

# The private half of the engine's key pair, from the _DEBUG block of
# code/_pk.cpp. Only a test needs it: it writes the encrypted archives that the
# reader is then asked to read back.
PRIVATE_EXPONENT = mixcrypt.der_integer(__import__("base64").b64decode(
    "AigKVje8mROcR8QixnxUEF5b29Curkq01DNDWCdOG99XBqH79OaCiTCB"))


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

    def test_reads_back_an_encrypted_archive(self):
        path = mixreader.write_mix(self.root / "MOVIES01.MIX", self.members,
                                   private_exponent=PRIVATE_EXPONENT)
        with mixreader.MixFile(path) as archive:
            self.assertEqual(archive.count, len(self.members))
            for name, data in self.members.items():
                self.assertEqual(archive.read(name), data, name)

    def test_an_encrypted_archive_places_its_data_after_whole_blocks(self):
        path = mixreader.write_mix(self.root / "E.MIX", self.members,
                                   private_exponent=PRIVATE_EXPONENT)
        key = mixcrypt.PublicKey.engine_key()
        total = mixreader.HEADER_SIZE + len(self.members) * mixreader.ENTRY_SIZE
        blocks = -(-total // blowfish.BLOCK_SIZE)
        with mixreader.MixFile(path) as archive:
            self.assertEqual(archive.data_start,
                             4 + key.encrypted_key_length()
                             + blocks * blowfish.BLOCK_SIZE)

    def test_reports_an_encrypted_archive_that_stops_short(self):
        path = self.root / "E.MIX"
        path.write_bytes(struct.pack("<hh", 0, mixreader.FLAG_ENCRYPTED) + b"\0" * 64)
        with self.assertRaises(mixreader.MixError):
            mixreader.MixFile(path)

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


class BlowfishTests(unittest.TestCase):
    """Checked against the published Blowfish ECB vectors."""

    VECTORS = (
        ("0000000000000000", "0000000000000000", "4EF997456198DD78"),
        ("FFFFFFFFFFFFFFFF", "FFFFFFFFFFFFFFFF", "51866FD5B85ECB8A"),
        ("3000000000000000", "1000000000000001", "7D856F9A613063F2"),
        ("1111111111111111", "1111111111111111", "2466DD878B963C9D"),
        ("0123456789ABCDEF", "1111111111111111", "61F9C3802281B096"),
        ("FEDCBA9876543210", "0123456789ABCDEF", "0ACEAB0FC6A0A28D"),
    )

    def test_encrypts_the_published_vectors(self):
        for key, plain, cipher in self.VECTORS:
            engine = blowfish.Blowfish(bytes.fromhex(key))
            self.assertEqual(engine.encrypt(bytes.fromhex(plain)).hex().upper(),
                             cipher, key)

    def test_decrypts_the_published_vectors(self):
        for key, plain, cipher in self.VECTORS:
            engine = blowfish.Blowfish(bytes.fromhex(key))
            self.assertEqual(engine.decrypt(bytes.fromhex(cipher)).hex().upper(),
                             plain, key)

    def test_refuses_a_partial_block(self):
        engine = blowfish.Blowfish(b"key")
        with self.assertRaises(ValueError):
            engine.decrypt(b"1234567")

    def test_refuses_an_empty_key(self):
        with self.assertRaises(ValueError):
            blowfish.Blowfish(b"")


class KeyExchangeTests(unittest.TestCase):
    """The block sizes and the key pair the engine carries."""

    def setUp(self):
        self.key = mixcrypt.PublicKey.engine_key()

    def test_block_sizes_match_the_engine(self):
        self.assertEqual(self.key.plain_block_size, 39)
        self.assertEqual(self.key.crypt_block_size, 40)
        self.assertEqual(self.key.encrypted_key_length(), 80)
        self.assertEqual(self.key.plain_key_length(), 78)

    def test_the_engines_key_pair_round_trips(self):
        private = mixcrypt.PublicKey(self.key.modulus, PRIVATE_EXPONENT)
        plain = bytes(range(self.key.plain_key_length()))
        self.assertEqual(self.key.decrypt(private.encrypt(plain)), plain)

    def test_recovers_a_blowfish_key_from_its_block(self):
        private = mixcrypt.PublicKey(self.key.modulus, PRIVATE_EXPONENT)
        secret = bytes(range(200, 200 + blowfish.KEY_SIZE))
        block = private.encrypt(secret.ljust(self.key.plain_key_length(), b"\0"))
        engine = mixcrypt.blowfish_for(block, self.key)
        expected = blowfish.Blowfish(secret)
        self.assertEqual(engine.encrypt(b"12345678"), expected.encrypt(b"12345678"))

    def test_reports_a_truncated_key_block(self):
        with self.assertRaises(ValueError):
            mixcrypt.blowfish_for(b"\0" * 40, self.key)

    def test_der_integer_reads_both_length_forms(self):
        self.assertEqual(mixcrypt.der_integer(bytes([0x02, 0x02, 0x01, 0x00])), 256)
        self.assertEqual(
            mixcrypt.der_integer(bytes([0x02, 0x81, 0x02, 0x01, 0x00])), 256)

    def test_der_integer_rejects_other_tags(self):
        with self.assertRaises(ValueError):
            mixcrypt.der_integer(bytes([0x03, 0x01, 0x00]))


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


class ModelTests(unittest.TestCase):
    """A model the upscaler cannot load has to be reported before it is launched."""

    def setUp(self):
        self.temp = TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.addCleanup(self.temp.cleanup)
        self.models = self.root / "models"
        self.models.mkdir()

    def config_for(self, model="realesrgan-x4plus"):
        return make_config(self.root, model_path=str(self.models),
                           upscale_model=model)

    def place(self, name):
        for suffix in (".param", ".bin"):
            (self.models / f"{name}{suffix}").write_bytes(b"")

    def test_accepts_a_model_whose_pair_is_present(self):
        self.place("realesrgan-x4plus")
        self.assertEqual(upscale_stage.check_model(self.config_for()), self.models)

    def test_names_the_models_that_are_there(self):
        self.place("realesr-animevideov3")
        self.place("realesrnet-x4plus")
        with self.assertRaises(common.PipelineError) as caught:
            upscale_stage.check_model(self.config_for())
        message = str(caught.exception)
        self.assertIn("realesrgan-x4plus.param", message)
        self.assertIn("realesr-animevideov3", message)
        self.assertIn("realesrnet-x4plus", message)

    def test_accepts_a_model_that_carries_the_scale_in_its_name(self):
        self.place("realesr-animevideov3-x4")
        config = make_config(self.root, model_path=str(self.models),
                             upscale_model="realesr-animevideov3",
                             upscale_factor=4)
        self.assertEqual(upscale_stage.check_model(config), self.models)

    def test_rejects_a_scaled_model_at_a_scale_it_has_no_file_for(self):
        self.place("realesr-animevideov3-x4")
        config = make_config(self.root, model_path=str(self.models),
                             upscale_model="realesr-animevideov3",
                             upscale_factor=2)
        with self.assertRaises(common.PipelineError) as caught:
            upscale_stage.check_model(config)
        self.assertIn("realesr-animevideov3-x2.param", str(caught.exception))

    def test_reports_a_folder_holding_no_model(self):
        with self.assertRaises(common.PipelineError) as caught:
            upscale_stage.check_model(self.config_for())
        self.assertIn("no .param file at all", str(caught.exception))

    def test_reports_a_half_present_model(self):
        (self.models / "realesrgan-x4plus.param").write_bytes(b"")
        with self.assertRaises(common.PipelineError) as caught:
            upscale_stage.check_model(self.config_for())
        self.assertIn("realesrgan-x4plus.bin", str(caught.exception))

    def test_reports_a_directory_that_is_not_there(self):
        config = make_config(self.root, model_path=str(self.root / "absent"))
        with self.assertRaises(common.PipelineError) as caught:
            upscale_stage.check_model(config)
        self.assertIn("is not a directory", str(caught.exception))

    def test_the_environment_supplies_the_folder_when_the_setting_is_empty(self):
        config = make_config(self.root, model_path="")
        with unittest.mock.patch.dict(
                os.environ, {"OPENTS_REALESRGAN_MODELS": str(self.models)}):
            self.assertEqual(upscale_stage.model_directory(config), self.models)

    def test_the_setting_wins_over_the_environment(self):
        config = make_config(self.root, model_path=str(self.models))
        with unittest.mock.patch.dict(
                os.environ, {"OPENTS_REALESRGAN_MODELS": str(self.root / "other")}):
            self.assertEqual(upscale_stage.model_directory(config), self.models)

    def test_falls_back_to_the_folder_beside_the_executable(self):
        config = make_config(self.root, model_path="")
        fake = self.root / "realesrgan-ncnn-vulkan"
        fake.write_bytes(b"")
        with unittest.mock.patch.dict(os.environ, {"OPENTS_REALESRGAN": str(fake)},
                                      clear=False):
            os.environ.pop("OPENTS_REALESRGAN_MODELS", None)
            self.assertEqual(upscale_stage.model_directory(config), self.models)


class CompleteImageTests(unittest.TestCase):
    """A frame left half written by an interrupted run must not count as done."""

    def setUp(self):
        self.temp = TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.addCleanup(self.temp.cleanup)

    def write(self, name, data):
        path = self.root / name
        path.write_bytes(data)
        return path

    def test_accepts_a_png_that_ends_with_its_terminator(self):
        self.assertTrue(upscale_stage.is_complete(
            self.write("a.png", b"\x89PNG" + b"x" * 40 + b"IEND\xae\x42\x60\x82")))

    def test_rejects_a_truncated_png(self):
        self.assertFalse(upscale_stage.is_complete(
            self.write("b.png", b"\x89PNG" + b"x" * 40)))

    def test_rejects_an_empty_file(self):
        self.assertFalse(upscale_stage.is_complete(self.write("c.png", b"")))

    def test_accepts_a_jpeg_that_ends_with_its_marker(self):
        self.assertTrue(upscale_stage.is_complete(
            self.write("d.jpg", b"\xff\xd8" + b"x" * 20 + b"\xff\xd9")))

    def test_rejects_a_truncated_jpeg(self):
        self.assertFalse(upscale_stage.is_complete(
            self.write("e.jpg", b"\xff\xd8" + b"x" * 20)))


class ExitCodeTests(unittest.TestCase):
    def test_windows_status_codes_carry_their_hex(self):
        self.assertEqual(common.exit_code_text(3221226505),
                         "3221226505 (0xC0000409)")

    def test_ordinary_codes_stay_plain(self):
        self.assertEqual(common.exit_code_text(1), "1")
        self.assertEqual(common.exit_code_text(0), "0")


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

    def test_dump_records_the_rate_beside_the_frames(self):
        _media, frames, _audio = dump_frames.dump(self.config, "PROOF", self.clip)
        info = common.read_frame_info(frames)
        self.assertIsNotNone(info)
        self.assertAlmostEqual(info.fps, SOURCE_FPS, places=3)
        self.assertEqual(info.frames, SOURCE_FRAMES)
        self.assertAlmostEqual(common.frame_rate(frames), SOURCE_FPS, places=3)

    def test_frame_rate_falls_back_when_nothing_was_recorded(self):
        self.assertEqual(common.frame_rate(self.root / "absent"), 15.0)

    def test_the_sidecar_is_not_counted_as_a_frame(self):
        _media, frames, _audio = dump_frames.dump(self.config, "PROOF", self.clip)
        self.assertEqual(len(common.frame_files(frames)), SOURCE_FRAMES)

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

    def test_upscale_writes_into_the_target_directory_itself(self):
        dump_frames.dump(self.config, "PROOF", self.clip)
        target, _ran, _rate = upscale_stage.upscale(self.config, "PROOF",
                                                    backend="lanczos")
        self.assertEqual(target, self.config.upscaled_dir("PROOF"))
        self.assertEqual(len(common.frame_files(target)), SOURCE_FRAMES)
        leftovers = [p.name for p in target.parent.iterdir() if p.name.startswith(".")]
        self.assertEqual(leftovers, [])

    def test_upscale_redoes_a_frame_left_half_written(self):
        dump_frames.dump(self.config, "PROOF", self.clip)
        upscale_stage.upscale(self.config, "PROOF", backend="lanczos")
        target = self.config.upscaled_dir("PROOF")
        victim = common.frame_files(target)[3]
        victim.write_bytes(victim.read_bytes()[:-20])
        _target, ran, _rate = upscale_stage.upscale(self.config, "PROOF",
                                                    backend="lanczos")
        self.assertEqual(ran, 1)
        self.assertTrue(upscale_stage.is_complete(victim))

    def test_upscale_does_no_work_when_everything_is_present(self):
        dump_frames.dump(self.config, "PROOF", self.clip)
        upscale_stage.upscale(self.config, "PROOF", backend="lanczos")
        _target, ran, _rate = upscale_stage.upscale(self.config, "PROOF",
                                                    backend="lanczos")
        self.assertEqual(ran, 0)

    def test_upscale_denoises_by_default(self):
        dump_frames.dump(self.config, "PROOF", self.clip)
        self.assertTrue(self.config.denoise_filter)
        upscale_stage.upscale(self.config, "PROOF", backend="lanczos")
        self.assertEqual(len(common.frame_files(self.config.denoised_dir("PROOF"))),
                         SOURCE_FRAMES)

    def test_no_denoise_skips_the_filter(self):
        dump_frames.dump(self.config, "PROOF", self.clip)
        upscale_stage.upscale(self.config, "PROOF", backend="lanczos",
                              use_denoise=False)
        self.assertFalse(self.config.denoised_dir("PROOF").is_dir())

    def test_an_empty_filter_leaves_the_frames_alone(self):
        config = make_config(self.root, denoise_filter="")
        dump_frames.dump(config, "PROOF", self.clip)
        upscale_stage.upscale(config, "PROOF", backend="lanczos")
        self.assertFalse(config.denoised_dir("PROOF").is_dir())
        self.assertEqual(len(common.frame_files(config.upscaled_dir("PROOF"))),
                         SOURCE_FRAMES)

    def test_the_shipped_configuration_names_a_model_and_a_filter(self):
        shipped = common.Config.load()
        self.assertEqual(shipped.upscale_model, "realesr-animevideov3")
        self.assertIn("deblock", shipped.denoise_filter)

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

    def test_encode_applies_a_post_filter(self):
        dump_frames.dump(self.config, "PROOF", self.clip)
        upscale_stage.upscale(self.config, "PROOF", backend="lanczos")
        plain = encode_stage.encode(self.config, "PROOF", fps=SOURCE_FPS,
                                    post_filter="", target=self.root / "plain.mp4")
        filtered = encode_stage.encode(self.config, "PROOF", fps=SOURCE_FPS,
                                       post_filter="hue=s=0",
                                       target=self.root / "grey.mp4")
        self.assertNotEqual(common.md5_of(plain), common.md5_of(filtered))

    def test_encode_reports_a_bad_post_filter(self):
        dump_frames.dump(self.config, "PROOF", self.clip)
        upscale_stage.upscale(self.config, "PROOF", backend="lanczos")
        with self.assertRaises(common.PipelineError):
            encode_stage.encode(self.config, "PROOF", fps=SOURCE_FPS,
                                post_filter="nosuchfilter=1",
                                target=self.root / "bad.mp4")

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

    def test_trial_encodes_one_clip_for_each_recipe(self):
        dump_frames.dump(self.config, "PROOF", self.clip)
        args = _Args(movie="PROOF", start=0, length=4, fps=None,
                     only=["plain", "deblock"], backend="lanczos", force=False)
        self.assertEqual(pipeline.command_trial(self.config, args), 0)
        root = self.config.trial_dir("PROOF")
        clips = sorted(p.name for p in root.iterdir() if p.suffix == ".mp4")
        self.assertEqual(clips, ["deblock.mp4", "plain.mp4"])
        for clip in clips:
            self.assertEqual(common.probe(root / clip).frames, 4)

    def test_trial_recipes_differ_in_their_output(self):
        dump_frames.dump(self.config, "PROOF", self.clip)
        args = _Args(movie="PROOF", start=0, length=4, fps=None,
                     only=["plain", "deblock"], backend="lanczos", force=False)
        pipeline.command_trial(self.config, args)
        root = self.config.trial_dir("PROOF")
        self.assertNotEqual(common.md5_of(root / "plain.mp4"),
                            common.md5_of(root / "deblock.mp4"))

    def test_trial_names_the_recipes_when_none_is_selected(self):
        dump_frames.dump(self.config, "PROOF", self.clip)
        args = _Args(movie="PROOF", start=0, length=4, only=["nonesuch"], fps=None,
                     backend="lanczos", force=False)
        with self.assertRaises(common.PipelineError) as caught:
            pipeline.command_trial(self.config, args)
        self.assertIn("plain", str(caught.exception))

    def test_trial_rejects_a_segment_outside_the_movie(self):
        dump_frames.dump(self.config, "PROOF", self.clip)
        args = _Args(movie="PROOF", start=SOURCE_FRAMES + 10, length=4, fps=None,
                     only=["plain"], backend="lanczos", force=False)
        with self.assertRaises(common.PipelineError):
            pipeline.command_trial(self.config, args)

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
