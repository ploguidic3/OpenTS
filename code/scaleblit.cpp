/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "scaleblit.h"

#include <cstring>


void Scale_Expand_8Bit(unsigned char const * source, int width, int height, unsigned char * dest, int scale)
{
	if (source == NULL || dest == NULL || width <= 0 || height <= 0 || scale < 1) {
		return;
	}

	int const destwidth = width * scale;

	for (int y = 0; y < height; y++) {
		unsigned char const * srcrow = source + y * width;
		unsigned char * destrow = dest + (y * scale) * destwidth;

		for (int x = 0; x < width; x++) {
			std::memset(destrow + x * scale, srcrow[x], scale);
		}

		for (int copy = 1; copy < scale; copy++) {
			std::memcpy(destrow + copy * destwidth, destrow, destwidth);
		}
	}
}


int Scale_Encoded_RLE_Bound(int width, int height)
{
	if (width <= 0 || height <= 0) {
		return(0);
	}

	// A row is at worst alternating literals and single-byte runs, each run costing its code
	// and its count, plus the row's own length prefix.
	return(height * (width * 2 + (int)sizeof(unsigned short)));
}


int Scale_Encode_RLE_Frame(unsigned char const * source, int width, int height, unsigned char * dest)
{
	if (source == NULL || dest == NULL || width <= 0 || height <= 0) {
		return(0);
	}

	unsigned char * out = dest;

	for (int y = 0; y < height; y++) {
		unsigned char const * in = source + (std::size_t)y * width;
		unsigned char * prefix = out;
		out += sizeof(unsigned short);

		int x = 0;
		while (x < width) {
			if (in[x] != 0) {
				*out++ = in[x];
				x++;
				continue;
			}

			// The transparent index is the run code, so a run of it carries its own count and
			// a count over 255 is emitted as several runs.
			int run = 0;
			while (x + run < width && in[x + run] == 0 && run < 255) {
				run++;
			}

			*out++ = 0;
			*out++ = (unsigned char)run;
			x += run;
		}

		std::size_t const rowbytes = (std::size_t)(out - prefix);
		if (rowbytes > 0xFFFF) {
			return(0);
		}

		unsigned short const stored = (unsigned short)rowbytes;
		std::memcpy(prefix, &stored, sizeof(stored));
	}

	return((int)(out - dest));
}


bool Scale_Decode_RLE_Frame(void const * data, int data_size, int width, int height, unsigned char * dest)
{
	if (dest == NULL || width <= 0 || height <= 0) {
		return(false);
	}

	std::memset(dest, 0, (std::size_t)width * height);

	if (data == NULL || data_size < 0) {
		return(false);
	}

	unsigned char const * source = (unsigned char const *)data;
	bool const bounded = data_size > 0;
	int remaining = data_size;

	for (int y = 0; y < height; y++) {

		// Every row is prefixed with its own total length, counting the prefix itself.
		if (bounded && remaining < (int)sizeof(unsigned short)) {
			return(false);
		}

		unsigned short rowbytes;
		std::memcpy(&rowbytes, source, sizeof(rowbytes));
		if (rowbytes < sizeof(unsigned short) || (bounded && rowbytes > remaining)) {
			return(false);
		}

		unsigned char const * in = source + sizeof(unsigned short);
		int inleft = rowbytes - (int)sizeof(unsigned short);
		unsigned char * out = dest + (std::size_t)y * width;
		int outleft = width;

		while (inleft > 0) {
			unsigned char value = *in++;
			inleft--;

			if (value != 0) {

				// A row may encode past its width; the blitter stops at the width and so does this.
				if (outleft > 0) {
					*out++ = value;
					outleft--;
				}
				continue;
			}

			// A run code is always followed by its count.
			if (inleft == 0) {
				return(false);
			}

			int run = *in++;
			inleft--;
			if (run > outleft) {
				run = outleft;
			}

			std::memset(out, 0, run);
			out += run;
			outleft -= run;
		}

		source += rowbytes;
		remaining -= rowbytes;
	}

	return(true);
}
