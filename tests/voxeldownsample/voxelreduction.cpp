/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// Holds the reduction that turns a voxel drawn at twice its size back into palette indices
// to its coverage, house colour and blending rules. Needs no game data.

#include <cstdio>
#include <cstring>

#include "voxeldownsample.h"

namespace {

int Failures = 0;
int Checked = 0;

// Index one is black, two is white, three is the grey between them, and everything else
// outside the house range is a blue far from all three so a wrong pick is obvious. The house
// range is a red ramp, evenly spaced so an average of two of its entries names a third.
int const REMAP_START = 16;
int const REMAP_END = 31;

unsigned char Palette[VOXEL_DOWNSAMPLE_COLORS * 3];
unsigned char Nearest[VOXEL_DOWNSAMPLE_LUT_SIZE];

void Set_Color(int index, int red, int green, int blue)
{
	Palette[index * 3 + 0] = (unsigned char)red;
	Palette[index * 3 + 1] = (unsigned char)green;
	Palette[index * 3 + 2] = (unsigned char)blue;
}

void Build_Palette(void)
{
	for (int index = 0; index < VOXEL_DOWNSAMPLE_COLORS; index++) {
		Set_Color(index, 0, 0, 255);
	}

	Set_Color(0, 0, 0, 0);
	Set_Color(1, 0, 0, 0);
	Set_Color(2, 255, 255, 255);
	Set_Color(3, 128, 128, 128);

	for (int index = REMAP_START; index <= REMAP_END; index++) {
		Set_Color(index, (index - REMAP_START) * 17, 0, 0);
	}
}

void Check(bool condition, char const * what)
{
	std::printf("%-56s %s\n", what, condition ? "ok" : "FAILED");
	if (!condition) {
		Failures++;
	}
	Checked++;
}

unsigned char Reduce(unsigned char a, unsigned char b, unsigned char c, unsigned char d, VoxelDownsampleTables const & tables)
{
	unsigned char block[4] = { a, b, c, d };
	return(Voxel_Reduce_Block(block, 2, tables));
}

}


int main(void)
{
	Build_Palette();
	Voxel_Build_Nearest_Table(Palette, REMAP_START, REMAP_END, Nearest);

	VoxelDownsampleTables tables;
	tables.Palette = Palette;
	tables.Nearest = Nearest;
	tables.RemapStart = REMAP_START;
	tables.RemapEnd = REMAP_END;

	VoxelDownsampleTables commonest = tables;
	commonest.Nearest = NULL;

	Check(Reduce(0, 0, 0, 0, tables) == 0, "An empty block stays empty");
	Check(Reduce(0, 0, 0, 1, tables) == 0, "A block covered by one voxel stays empty");
	Check(Reduce(0, 0, 1, 1, tables) == 1, "A block covered by two voxels is drawn");
	Check(Reduce(2, 2, 2, 2, tables) == 2, "A block of one colour keeps that colour");

	Check(Reduce(1, 1, 2, 2, tables) == 3, "Black and white average to the grey between them");
	Check(Reduce(0, 1, 1, 2, tables) == 3, "An empty corner does not drag the average toward black");

	Check(Reduce(16, 16, 24, 24, tables) == 20, "Two house colours average to a third");
	Check(Reduce(16, 16, 1, 2, tables) == 16, "A mostly house coloured block keeps its house colour");
	Check(Reduce(1, 1, 2, 16, tables) == 1, "A mostly plain block does not take a house colour");

	bool ramp_is_house = true;
	for (int first = REMAP_START; first <= REMAP_END; first++) {
		for (int second = REMAP_START; second <= REMAP_END; second++) {
			unsigned char const result = Reduce((unsigned char)first, (unsigned char)first, (unsigned char)second, (unsigned char)second, tables);
			if (result < REMAP_START || result > REMAP_END) {
				ramp_is_house = false;
			}
		}
	}
	Check(ramp_is_house, "A house coloured block never leaves the house range");

	bool table_avoids_house = true;
	for (int entry = 0; entry < VOXEL_DOWNSAMPLE_LUT_SIZE; entry++) {
		if (Nearest[entry] == 0 || (Nearest[entry] >= REMAP_START && Nearest[entry] <= REMAP_END)) {
			table_avoids_house = false;
		}
	}
	Check(table_avoids_house, "The nearest colour table names no house colour");

	Check(Reduce(1, 1, 2, 2, commonest) == 1, "Without the table a block takes its commonest index");

	/*
	 * Four blocks of their own colour, laid out in a source wider than the part being read,
	 * so a mistake in either stride shows up as the wrong colour rather than the wrong size.
	 */
	unsigned char source[8 * 4];
	std::memset(source, 0, sizeof(source));
	unsigned char const corner[4] = { 1, 2, 3, 16 };
	for (int block = 0; block < 4; block++) {
		int const left = (block % 2) * 2;
		int const top = (block / 2) * 2;
		for (int y = 0; y < 2; y++) {
			for (int x = 0; x < 2; x++) {
				source[(top + y) * 8 + left + x] = corner[block];
			}
		}
	}

	unsigned char dest[2 * 3];
	std::memset(dest, 0xFF, sizeof(dest));
	Voxel_Downsample(source, 8, dest, 3, 2, 2, tables);

	Check(dest[0] == 1 && dest[1] == 2 && dest[3] == 3 && dest[4] == 16, "Each block reduces to its own colour");
	Check(dest[2] == 0xFF && dest[5] == 0xFF, "Reducing writes nothing past the width asked for");

	std::printf("checked %d cases, %d failures\n", Checked, Failures);

	return(Failures == 0 ? 0 : 1);
}
