/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// Exercises the shape expansion and the RLE frame decode behind the scaled tactical draw.
// Every byte it works on is synthesised here; it reads no game data.

#include <cstdio>
#include <cstring>
#include <vector>

#include "scaleblit.h"

namespace {

int Failures = 0;


void Check(bool condition, char const * what)
{
	std::printf("%-62s %s\n", what, condition ? "ok" : "FAILED");

	if (!condition) {
		Failures++;
	}
}


// Appends one row in the form the shape blitter walks: a total length counting its own
// prefix, then literal bytes with a zero followed by a count standing for a run of zeroes.
void Append_Row(std::vector<unsigned char> & out, std::vector<unsigned char> const & payload)
{
	unsigned short rowbytes = (unsigned short)(payload.size() + sizeof(unsigned short));
	unsigned char prefix[sizeof(unsigned short)];
	std::memcpy(prefix, &rowbytes, sizeof(rowbytes));

	out.insert(out.end(), prefix, prefix + sizeof(prefix));
	out.insert(out.end(), payload.begin(), payload.end());
}

}


int main(void)
{
	std::printf("Shape expansion\n\n");

	/*
	 * A frame with a transparent border, a remap-range index and a run of zeroes through the
	 * middle, so that every case the blitter cares about is present.
	 */
	unsigned char const frame[3 * 4] = {
		0,  17,  0,
		18,  0, 19,
		0,   0,  0,
		20, 21, 22,
	};

	std::vector<unsigned char> expanded(3 * 2 * 4 * 2, 0xFF);
	Scale_Expand_8Bit(frame, 3, 4, expanded.data(), 2);

	bool indices = true;
	bool transparent = true;
	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 6; x++) {
			unsigned char source = frame[(y / 2) * 3 + (x / 2)];
			unsigned char result = expanded[y * 6 + x];
			if (result != source) {
				indices = false;
			}
			if (source == 0 && result != 0) {
				transparent = false;
			}
		}
	}

	Check(indices, "Every doubled pixel keeps its source palette index");
	Check(transparent, "The transparent index stays transparent when doubled");

	std::vector<unsigned char> identity(3 * 4, 0xFF);
	Scale_Expand_8Bit(frame, 3, 4, identity.data(), 1);
	Check(std::memcmp(identity.data(), frame, sizeof(frame)) == 0, "Scale one copies the frame unchanged");

	std::vector<unsigned char> wide(4 * 3 * 1 * 3, 0xFF);
	Scale_Expand_8Bit(frame, 4, 1, wide.data(), 3);
	Check(wide[0] == 0 && wide[3] == 17 && wide[11] == 18, "A whole factor past two magnifies each row");

	/*
	 * The same picture, RLE encoded the way a shape file carries it.
	 */
	std::vector<unsigned char> encoded;
	Append_Row(encoded, {0, 1, 17, 0, 1});
	Append_Row(encoded, {18, 0, 1, 19});
	Append_Row(encoded, {0, 3});
	Append_Row(encoded, {20, 21, 22});

	std::vector<unsigned char> decoded(3 * 4, 0xFF);
	bool ok = Scale_Decode_RLE_Frame(encoded.data(), (int)encoded.size(), 3, 4, decoded.data());
	Check(ok, "A well formed RLE frame decodes");
	Check(std::memcmp(decoded.data(), frame, sizeof(frame)) == 0, "The decoded frame matches the uncompressed one");

	std::vector<unsigned char> unbounded(3 * 4, 0xFF);
	Check(Scale_Decode_RLE_Frame(encoded.data(), 0, 3, 4, unbounded.data()), "An unknown size decodes from the row prefixes alone");
	Check(std::memcmp(unbounded.data(), frame, sizeof(frame)) == 0, "The unbounded decode matches the bounded one");

	std::vector<unsigned char> roundtrip(3 * 2 * 4 * 2, 0xFF);
	Scale_Expand_8Bit(decoded.data(), 3, 4, roundtrip.data(), 2);
	Check(std::memcmp(roundtrip.data(), expanded.data(), expanded.size()) == 0, "Decoding then doubling matches doubling the raw frame");

	/*
	 * Shape data is cast in place and never validated, so the decode has to refuse anything
	 * that would write past the frame rather than trust the file.
	 */
	std::vector<unsigned char> truncated(encoded.begin(), encoded.begin() + 4);
	std::vector<unsigned char> guard(3 * 4, 0xFF);
	Check(!Scale_Decode_RLE_Frame(truncated.data(), (int)truncated.size(), 3, 4, guard.data()), "A truncated frame is refused");

	bool cleared = true;
	for (unsigned char value : guard) {
		if (value != 0) {
			cleared = false;
		}
	}
	Check(cleared, "A refused frame leaves the destination transparent");

	/*
	 * Real shape rows run past the frame width, most often with a trailing run of zeroes. The
	 * blitter stops at the width and ignores the rest, so a row like that is ordinary data and
	 * has to decode rather than be thrown out.
	 */
	std::vector<unsigned char> overrun;
	Append_Row(overrun, {1, 2, 3, 4, 5});
	std::vector<unsigned char> clipped(3, 0xFF);
	Check(Scale_Decode_RLE_Frame(overrun.data(), (int)overrun.size(), 3, 1, clipped.data()), "A row encoding past the frame width still decodes");
	Check(clipped[0] == 1 && clipped[1] == 2 && clipped[2] == 3, "The row is clipped to the frame width");

	std::vector<unsigned char> longrun;
	Append_Row(longrun, {17, 0, 200});
	Check(Scale_Decode_RLE_Frame(longrun.data(), (int)longrun.size(), 3, 1, clipped.data()), "A trailing zero run past the width still decodes");
	Check(clipped[0] == 17 && clipped[1] == 0 && clipped[2] == 0, "The trailing run fills only to the width");

	Check(Scale_Decode_RLE_Frame(overrun.data(), 0, 3, 1, clipped.data()), "An over-wide row decodes with no declared size too");

	std::vector<unsigned char> danglingrun;
	Append_Row(danglingrun, {17, 0});
	Check(!Scale_Decode_RLE_Frame(danglingrun.data(), (int)danglingrun.size(), 3, 1, guard.data()), "A run code with no count is refused");

	Check(!Scale_Decode_RLE_Frame(NULL, 0, 3, 1, guard.data()), "A frame with no data is refused");

	/*
	 * A magnified frame has to go back into the row format, because the RLE blitter is the only
	 * one that can take a depth shape alongside the shape it draws.
	 */
	std::vector<unsigned char> reencoded((std::size_t)Scale_Encoded_RLE_Bound(3, 4), 0xFF);
	int written = Scale_Encode_RLE_Frame(frame, 3, 4, reencoded.data());
	Check(written > 0 && written <= (int)reencoded.size(), "A frame encodes within the stated bound");

	std::vector<unsigned char> roundtripped(3 * 4, 0xFF);
	Check(Scale_Decode_RLE_Frame(reencoded.data(), written, 3, 4, roundtripped.data()), "The encoded frame decodes again");
	Check(std::memcmp(roundtripped.data(), frame, sizeof(frame)) == 0, "Encoding then decoding returns the original frame");

	/*
	 * The run count is one byte, so a row wider than a single run has to split into several.
	 */
	std::vector<unsigned char> wide_blank(300, 0);
	wide_blank[299] = 7;
	std::vector<unsigned char> blankenc((std::size_t)Scale_Encoded_RLE_Bound(300, 1), 0xFF);
	written = Scale_Encode_RLE_Frame(wide_blank.data(), 300, 1, blankenc.data());
	Check(written > 0, "A row longer than one run encodes");

	std::vector<unsigned char> blankout(300, 0xFF);
	Check(Scale_Decode_RLE_Frame(blankenc.data(), written, 300, 1, blankout.data()), "A split run decodes");
	Check(std::memcmp(blankout.data(), wide_blank.data(), wide_blank.size()) == 0, "A run split across codes keeps every pixel");

	std::vector<unsigned char> doubled(3 * 2 * 4 * 2, 0xFF);
	Scale_Expand_8Bit(frame, 3, 4, doubled.data(), 2);
	std::vector<unsigned char> doubledenc((std::size_t)Scale_Encoded_RLE_Bound(6, 8), 0xFF);
	written = Scale_Encode_RLE_Frame(doubled.data(), 6, 8, doubledenc.data());
	std::vector<unsigned char> doubledout(6 * 8, 0xFF);
	Check(written > 0 && Scale_Decode_RLE_Frame(doubledenc.data(), written, 6, 8, doubledout.data()), "A magnified frame encodes and decodes");
	Check(std::memcmp(doubledout.data(), doubled.data(), doubled.size()) == 0, "The magnified frame survives the round trip");

	Check(Scale_Encode_RLE_Frame(NULL, 3, 4, reencoded.data()) == 0, "Encoding refuses a frame with no data");

	std::printf("\n%d failure(s)\n", Failures);
	return(Failures == 0 ? 0 : 1);
}
