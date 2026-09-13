"""Reader for MIX archives, enough of the format to pull movies out of one.

The index holds a name checksum rather than a name, so a member is found by
hashing the name the same way the engine does. ``code/mixfile.cpp:537`` hashes
the upper-cased name through ``CRCEngine``, which accumulates whole four-byte
groups through ``CRC::Memory`` (``code/crc.cpp:298``, a reflected CRC-32) and
pads a short final group as ``CRCEngine::Add_Padding`` does
(``code/crc.h:128-133``).

Encrypted indices are detected and refused: the engine decrypts them with a key
that is not in this repository, so those archives have to be unpacked with XCC
Mixer instead. The README says so.
"""

from __future__ import annotations

import dataclasses
from pathlib import Path
import struct
import zlib


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
                raise MixError(
                    f"{self.path.name} has an encrypted index, which this reader does not "
                    "decrypt. Unpack it with XCC Mixer (see the README)."
                )
            header = self._handle.read(HEADER_SIZE)
        else:
            self._handle.seek(0)
            header = self._handle.read(HEADER_SIZE)
        if len(header) < HEADER_SIZE:
            raise MixError(f"{self.path} is too short to hold a MIX header")
        count, data_size = struct.unpack("<hi", header)
        if count < 0:
            raise MixError(f"{self.path} declares {count} members")
        index = self._handle.read(count * ENTRY_SIZE)
        if len(index) < count * ENTRY_SIZE:
            raise MixError(
                f"{self.path} declares {count} members but its index stops short"
            )
        members = [
            Member(*struct.unpack_from("<iii", index, i * ENTRY_SIZE))
            for i in range(count)
        ]
        data_start = self._handle.tell()
        return has_digest, count, data_size, data_start, members

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


def write_mix(path: Path, members: dict[str, bytes], extended: bool = False) -> Path:
    """Writes a MIX archive. Used by the tests to build a synthetic archive."""
    entries = []
    blob = bytearray()
    for name, data in members.items():
        entries.append(Member(name_key(name), len(blob), len(data)))
        blob += data
    entries.sort(key=lambda entry: entry.key)
    with open(path, "wb") as handle:
        if extended:
            handle.write(struct.pack("<hh", 0, 0))
        handle.write(struct.pack("<hi", len(entries), len(blob)))
        for entry in entries:
            handle.write(struct.pack("<iii", entry.key, entry.offset, entry.size))
        handle.write(blob)
    return Path(path)
