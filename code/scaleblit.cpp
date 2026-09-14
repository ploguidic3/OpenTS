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
				if (outleft == 0) {
					return(false);
				}
				*out++ = value;
				outleft--;
				continue;
			}

			if (inleft == 0) {
				return(false);
			}

			int run = *in++;
			inleft--;
			if (run > outleft) {
				return(false);
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
