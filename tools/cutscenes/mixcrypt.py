"""The key exchange guarding an encrypted MIX index.

An encrypted archive carries a Blowfish key of its own, itself encrypted with a
fixed key pair that the engine also carries: `code/_pk.cpp` holds the modulus,
and the exponent is the constant in `PKey::Fast_Exponent` (`code/pk.h:70`). The
game reads the archive with the public half, which is the direction implemented
here.

Blocks are whole numbers of bytes read little endian, following the way
`PKey::Decrypt` (`code/pk.cpp`) moves them in and out of its big integer.
"""

from __future__ import annotations

import base64

from blowfish import Blowfish, KEY_SIZE


# code/_pk.cpp: the [PublicKey] block of Keys[].
PUBLIC_MODULUS = "AihRvNoIbTn85FZRYNZRcT+i6KpU+maCsEqr3Q5q+LDB5tH7Tz2qQ38V"
FAST_EXPONENT = 65537


def der_integer(encoded: bytes) -> int:
    """Reads one DER integer, the form the key blocks are stored in."""
    if not encoded or encoded[0] != 0x02:
        raise ValueError("not a DER integer")
    if encoded[1] & 0x80 == 0:
        size, body = encoded[1], encoded[2:]
    else:
        width = encoded[1] & 0x7F
        if not 1 <= width <= 2:
            raise ValueError("unsupported DER length")
        size = int.from_bytes(encoded[2:2 + width], "big")
        body = encoded[2 + width:]
    if len(body) < size:
        raise ValueError("DER integer is truncated")
    return int.from_bytes(body[:size], "big")


class PublicKey:
    """The key the archives are read with, and the block sizes it implies."""

    def __init__(self, modulus: int, exponent: int = FAST_EXPONENT):
        self.modulus = modulus
        self.exponent = exponent
        self.bit_precision = modulus.bit_length() - 1
        self.plain_block_size = (self.bit_precision - 1) // 8
        self.crypt_block_size = self.plain_block_size + 1

    @classmethod
    def engine_key(cls) -> "PublicKey":
        return cls(der_integer(base64.b64decode(PUBLIC_MODULUS)))

    def block_count(self, plaintext_length: int) -> int:
        return ((plaintext_length - 1) // self.plain_block_size) + 1

    def encrypted_key_length(self) -> int:
        return self.crypt_block_size * self.block_count(KEY_SIZE)

    def plain_key_length(self) -> int:
        return self.plain_block_size * self.block_count(KEY_SIZE)

    def decrypt(self, data: bytes) -> bytes:
        """Decrypts whole blocks. A trailing partial block is ignored."""
        out = bytearray()
        for offset in range(0, len(data) - self.crypt_block_size + 1,
                            self.crypt_block_size):
            block = int.from_bytes(data[offset:offset + self.crypt_block_size], "little")
            plain = pow(block, self.exponent, self.modulus)
            out += plain.to_bytes(self.plain_block_size + 8, "little")[:self.plain_block_size]
        return bytes(out)

    def encrypt(self, data: bytes) -> bytes:
        """The other direction, which only a holder of the private exponent can do."""
        out = bytearray()
        for offset in range(0, len(data) - self.plain_block_size + 1,
                            self.plain_block_size):
            block = int.from_bytes(data[offset:offset + self.plain_block_size], "little")
            cipher = pow(block, self.exponent, self.modulus)
            out += cipher.to_bytes(self.crypt_block_size + 8, "little")[:self.crypt_block_size]
        return bytes(out)


def blowfish_for(key_block: bytes, key: PublicKey | None = None) -> Blowfish:
    """Returns the engine an archive's encrypted key block describes."""
    key = key or PublicKey.engine_key()
    if len(key_block) < key.encrypted_key_length():
        raise ValueError("the encrypted key block is truncated")
    return Blowfish(key.decrypt(key_block)[:KEY_SIZE])
