"""Reader for MIX archives, enough of the format to pull movies out of one.

The index holds a name checksum rather than a name, so a member is found by
hashing the name the same way the engine does. ``code/mixfile.cpp:537`` hashes
the upper-cased name through ``CRCEngine``, which accumulates whole four-byte
groups through ``CRC::Memory`` (``code/crc.cpp:298``, a reflected CRC-32) and
pads a short final group as ``CRCEngine::Add_Padding`` does
(``code/crc.h:128-133``).

An encrypted index is decrypted the way the engine does it, through the key pair
in `code/_pk.cpp`; `mixcrypt.py` covers that half. The member data itself is
never encrypted.
"""
# Copied unchanged from tools/cutscenes; the two copies can be merged when both land.


from __future__ import annotations

import dataclasses
from pathlib import Path
import struct
import zlib

from blowfish import BLOCK_SIZE, Blowfish
import mixcrypt


HEADER_SIZE = 6
ENTRY_SIZE = 12
FLAG_DIGEST = 0x01
FLAG_ENCRYPTED = 0x02


class MixError(RuntimeError):
    pass


def name_key(name: str) -> int:
    """Returns the signed index checksum the engine stores for a member name."""
    data = bytearray(name.upper().encode("ascii", "replace"))
    remainder = len(data) % 4
    if remainder:
        first = data[len(data) - remainder]
        data.append(remainder)
        while len(data) % 4:
            data.append(first)
    value = zlib.crc32(bytes(data)) & 0xFFFFFFFF
    return value - (1 << 32) if value >= (1 << 31) else value


@dataclasses.dataclass(frozen=True)
class Member:
    key: int
    offset: int  # From the start of the data block.
    size: int


class MixFile:
    """Opens a MIX archive and reads members out of it by name.

    Members are addressed by checksum, so listing reports keys and sizes only;
    ``read(name)`` is the way to get a member whose name is known.
    """

    def __init__(self, path: Path):
        self.path = Path(path)
        self._handle = open(self.path, "rb")
        try:
            self.has_digest, self.count, self.data_size, self.data_start, self.members = \
                self._read_index()
        except Exception:
            self._handle.close()
            raise

    def _read_index(self):
        head = self._handle.read(4)
        if len(head) < 4:
            raise MixError(f"{self.path} is too short to hold a MIX header")
        first, second = struct.unpack("<hh", head)
        has_digest = False
        if first == 0:
            has_digest = bool(second & FLAG_DIGEST)
            if second & FLAG_ENCRYPTED:
                header, index, data_start = self._read_encrypted_index()
                count, data_size, members = self._members_from(header, index)
                return has_digest, count, data_size, data_start, members
            header = self._handle.read(HEADER_SIZE)
        else:
            self._handle.seek(0)
            header = self._handle.read(HEADER_SIZE)
        if len(header) < HEADER_SIZE:
            raise MixError(f"{self.path} is too short to hold a MIX header")
        count = struct.unpack_from("<h", header)[0]
        index = self._handle.read(count * ENTRY_SIZE) if count > 0 else b""
        count, data_size, members = self._members_from(header, index)
        return has_digest, count, data_size, self._handle.tell(), members

    def _read_encrypted_index(self):
        """Decrypts the index, leaving the member data where it is.

        The header and index are one run of Blowfish blocks, and the data
        follows the last whole block of that run.
        """
        key = mixcrypt.PublicKey.engine_key()
        key_block = self._handle.read(key.encrypted_key_length())
        try:
            engine = mixcrypt.blowfish_for(key_block, key)
        except ValueError as error:
            raise MixError(f"{self.path.name}: {error}") from error

        first = self._read_blocks(engine, 1)
        count = struct.unpack_from("<h", first)[0]
        if count < 0:
            raise MixError(f"{self.path} declares {count} members")
        total = HEADER_SIZE + count * ENTRY_SIZE
        blocks = -(-total // BLOCK_SIZE)
        plain = first + self._read_blocks(engine, blocks - 1)
        data_start = 4 + key.encrypted_key_length() + blocks * BLOCK_SIZE
        return plain[:HEADER_SIZE], plain[HEADER_SIZE:total], data_start

    def _read_blocks(self, engine: Blowfish, blocks: int) -> bytes:
        if blocks <= 0:
            return b""
        raw = self._handle.read(blocks * BLOCK_SIZE)
        if len(raw) < blocks * BLOCK_SIZE:
            raise MixError(f"{self.path} ends inside its encrypted index")
        return engine.decrypt(raw)

    def _members_from(self, header: bytes, index: bytes):
        if len(header) < HEADER_SIZE:
            raise MixError(f"{self.path} is too short to hold a MIX header")
        count, data_size = struct.unpack("<hi", header)
        if count < 0:
            raise MixError(f"{self.path} declares {count} members")
        if len(index) < count * ENTRY_SIZE:
            raise MixError(
                f"{self.path} declares {count} members but its index stops short"
            )
        members = [
            Member(*struct.unpack_from("<iii", index, i * ENTRY_SIZE))
            for i in range(count)
        ]
        return count, data_size, members

    def __enter__(self) -> "MixFile":
        return self

    def __exit__(self, *_exc) -> None:
        self.close()

    def close(self) -> None:
        if not self._handle.closed:
            self._handle.close()

    def find(self, name: str) -> Member | None:
        key = name_key(name)
        for member in self.members:
            if member.key == key:
                return member
        return None

    def read(self, name: str) -> bytes:
        member = self.find(name)
        if member is None:
            raise MixError(f"{name} is not a member of {self.path.name}")
        return self.read_member(member)

    def read_member(self, member: Member) -> bytes:
        self._handle.seek(self.data_start + member.offset)
        data = self._handle.read(member.size)
        if len(data) != member.size:
            raise MixError(
                f"{self.path.name} member at {member.offset} claims {member.size} bytes "
                f"but only {len(data)} are present"
            )
        return data

    def resolve(self, names) -> dict[str, Member]:
        """Maps each candidate name that the archive holds to its index entry."""
        by_key = {member.key: member for member in self.members}
        found = {}
        for name in names:
            member = by_key.get(name_key(name))
            if member is not None:
                found[name] = member
        return found


def write_mix(path: Path, members: dict[str, bytes], extended: bool = False,
              private_exponent: int | None = None,
              blowfish_key: bytes | None = None) -> Path:
    """Writes a MIX archive. Used by the tests to build a synthetic archive.

    Passing private_exponent writes an encrypted index, which needs the private
    half of the engine's key pair and is therefore only ever done by a test.
    """
    entries = []
    blob = bytearray()
    for name, data in members.items():
        entries.append(Member(name_key(name), len(blob), len(data)))
        blob += data
    entries.sort(key=lambda entry: entry.key)

    header = struct.pack("<hi", len(entries), len(blob))
    index = b"".join(struct.pack("<iii", e.key, e.offset, e.size) for e in entries)

    with open(path, "wb") as handle:
        if private_exponent is not None:
            public = mixcrypt.PublicKey.engine_key()
            private = mixcrypt.PublicKey(public.modulus, private_exponent)
            key = blowfish_key or bytes(range(mixcrypt.KEY_SIZE))
            padded = key.ljust(private.plain_key_length(), b"\0")
            plain = (header + index).ljust(
                -(-len(header + index) // BLOCK_SIZE) * BLOCK_SIZE, b"\0")
            handle.write(struct.pack("<hh", 0, FLAG_ENCRYPTED))
            handle.write(private.encrypt(padded))
            handle.write(Blowfish(key).encrypt(plain))
        else:
            if extended:
                handle.write(struct.pack("<hh", 0, 0))
            handle.write(header)
            handle.write(index)
        handle.write(blob)
    return Path(path)
