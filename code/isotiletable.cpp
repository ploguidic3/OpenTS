/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "isotiletable.h"

#include <cstring>


namespace
{

// The diamond widens by this much a row until it reaches the full tile, then narrows again.
constexpr int SPAN_STEP = 4;
constexpr int SPAN_PEAK_ROW = 11;
constexpr int SPAN_LAST_ROW = 22;


int Base_Span(int row)
{
	if (row < 0 || row > SPAN_LAST_ROW) {
		return(0);
	}
	if (row <= SPAN_PEAK_ROW) {
		return(SPAN_STEP * (row + 1));
	}
	return(SPAN_STEP * (SPAN_LAST_ROW + 1 - row));
}

}


int Iso_Row_Span(int row, int scale)
{
	if (scale < 1) {
		return(0);
	}

	return(Base_Span(row / scale) * scale);
}


void Iso_Build_Row_Bases(int scale, int * bases)
{
	if (bases == NULL || scale < 1) {
		return;
	}

	int const rows = ISO_TILE_BASE_ROWS * scale;
	int offset = 0;

	for (int row = 0; row < rows; row++) {
		bases[row] = offset;
		offset += Iso_Row_Span(row, scale);
	}
}


void Iso_Build_Mask(int scale, int row_offset, unsigned char * mask)
{
	if (mask == NULL || scale < 1) {
		return;
	}

	int const width = ISO_TILE_BASE_WIDTH * scale;
	int const rows = ISO_TILE_BASE_ROWS * scale;

	std::memset(mask, ISO_TILE_OUTSIDE, (std::size_t)width * rows);

	for (int row = 0; row < rows; row++) {
		int const span = Iso_Row_Span(row - row_offset, scale);
		if (span <= 0) {
			continue;
		}

		int const first = (width - span) / 2;
		std::memset(mask + (std::size_t)row * width + first, ISO_TILE_INSIDE, span);
	}
}


void Iso_Expand_Tile(unsigned char const * source, unsigned char * dest, int scale)
{
	if (source == NULL || dest == NULL || scale < 1) {
		return;
	}

	int const rows = ISO_TILE_BASE_ROWS * scale;
	int sourceoffset = 0;
	int lastsource = -1;
	unsigned char * out = dest;

	for (int row = 0; row < rows; row++) {
		int const sourcerow = row / scale;
		int const basespan = Iso_Row_Span(sourcerow, 1);

		// Every source row is laid down scale times, so its offset only advances between them.
		if (sourcerow != lastsource) {
			if (lastsource >= 0) {
				sourceoffset += Iso_Row_Span(lastsource, 1);
			}
			lastsource = sourcerow;
		}

		for (int pixel = 0; pixel < basespan; pixel++) {
			std::memset(out, source[sourceoffset + pixel], scale);
			out += scale;
		}
	}
}
