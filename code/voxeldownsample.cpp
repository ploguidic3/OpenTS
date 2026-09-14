/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "voxeldownsample.h"

#include <cstddef>


namespace
{

constexpr int LUT_STEP = 256 >> VOXEL_DOWNSAMPLE_LUT_BITS;
constexpr int LUT_SHIFT = 8 - VOXEL_DOWNSAMPLE_LUT_BITS;

inline bool Is_Remap(int index, VoxelDownsampleTables const & tables)
{
	return(index >= tables.RemapStart && index <= tables.RemapEnd);
}

inline int Lut_Index(int red, int green, int blue)
{
	return(((red >> LUT_SHIFT) << (VOXEL_DOWNSAMPLE_LUT_BITS * 2)) | ((green >> LUT_SHIFT) << VOXEL_DOWNSAMPLE_LUT_BITS) | (blue >> LUT_SHIFT));
}

unsigned char Commonest(unsigned char const * sample, int count)
{
	unsigned char best = sample[0];
	int bestcount = 0;

	for (int i = 0; i < count; i++) {
		int seen = 0;
		for (int j = 0; j < count; j++) {
			if (sample[j] == sample[i]) {
				seen++;
			}
		}
		if (seen > bestcount) {
			bestcount = seen;
			best = sample[i];
		}
	}

	return(best);
}

// Searches the palette directly rather than through the table, because the table refuses to
// name a house colour.
unsigned char Nearest_In_Range(unsigned char const * palette, int first, int last, int red, int green, int blue)
{
	unsigned char best = (unsigned char)first;
	long bestdistance = -1;

	for (int index = first; index <= last; index++) {
		int dr = (int)palette[index * 3 + 0] - red;
		int dg = (int)palette[index * 3 + 1] - green;
		int db = (int)palette[index * 3 + 2] - blue;
		long distance = (long)dr * dr + (long)dg * dg + (long)db * db;

		if (bestdistance < 0 || distance < bestdistance) {
			bestdistance = distance;
			best = (unsigned char)index;
		}
	}

	return(best);
}

}


void Voxel_Build_Nearest_Table(unsigned char const * palette, int remapstart, int remapend, unsigned char * table)
{
	for (int red = 0; red < (1 << VOXEL_DOWNSAMPLE_LUT_BITS); red++) {
		for (int green = 0; green < (1 << VOXEL_DOWNSAMPLE_LUT_BITS); green++) {
			for (int blue = 0; blue < (1 << VOXEL_DOWNSAMPLE_LUT_BITS); blue++) {

				// The centre of the bucket, so a colour is not always rounded downward.
				int want_red = red * LUT_STEP + LUT_STEP / 2;
				int want_green = green * LUT_STEP + LUT_STEP / 2;
				int want_blue = blue * LUT_STEP + LUT_STEP / 2;

				unsigned char best = 1;
				long bestdistance = -1;

				for (int index = 1; index < VOXEL_DOWNSAMPLE_COLORS; index++) {
					if (index >= remapstart && index <= remapend) {
						continue;
					}

					int dr = (int)palette[index * 3 + 0] - want_red;
					int dg = (int)palette[index * 3 + 1] - want_green;
					int db = (int)palette[index * 3 + 2] - want_blue;
					long distance = (long)dr * dr + (long)dg * dg + (long)db * db;

					if (bestdistance < 0 || distance < bestdistance) {
						bestdistance = distance;
						best = (unsigned char)index;
					}
				}

				table[Lut_Index(want_red, want_green, want_blue)] = best;
			}
		}
	}
}


unsigned char Voxel_Reduce_Block(unsigned char const * block, int stride, VoxelDownsampleTables const & tables)
{
	unsigned char sample[4];
	int count = 0;

	for (int row = 0; row < 2; row++) {
		for (int column = 0; column < 2; column++) {
			unsigned char index = block[row * stride + column];
			if (index != 0) {
				sample[count] = index;
				count++;
			}
		}
	}

	// Half coverage keeps the model's area. Demanding more erodes a gun barrel or an
	// antenna away entirely.
	if (count < 2) {
		return(0);
	}

	bool uniform = true;
	int remapcount = 0;
	for (int i = 0; i < count; i++) {
		if (sample[i] != sample[0]) {
			uniform = false;
		}
		if (Is_Remap(sample[i], tables)) {
			remapcount++;
		}
	}

	if (uniform) {
		return(sample[0]);
	}

	if (tables.Nearest == NULL) {
		return(Commonest(sample, count));
	}

	// A mixed colour would land outside the house range, or inside it by accident.
	if (remapcount != 0 && remapcount != count) {
		return(Commonest(sample, count));
	}

	int red = 0;
	int green = 0;
	int blue = 0;
	for (int i = 0; i < count; i++) {
		red += tables.Palette[sample[i] * 3 + 0];
		green += tables.Palette[sample[i] * 3 + 1];
		blue += tables.Palette[sample[i] * 3 + 2];
	}
	red /= count;
	green /= count;
	blue /= count;

	if (remapcount == count) {
		return(Nearest_In_Range(tables.Palette, tables.RemapStart, tables.RemapEnd, red, green, blue));
	}

	return(tables.Nearest[Lut_Index(red, green, blue)]);
}


void Voxel_Downsample(unsigned char const * source, int sourcestride, unsigned char * dest, int deststride, int width, int height, VoxelDownsampleTables const & tables)
{
	for (int y = 0; y < height; y++) {
		unsigned char const * row = source + (y * 2) * sourcestride;
		unsigned char * out = dest + y * deststride;

		for (int x = 0; x < width; x++) {
			out[x] = Voxel_Reduce_Block(row + x * 2, sourcestride, tables);
		}
	}
}
