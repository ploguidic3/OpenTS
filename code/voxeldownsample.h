/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once


// Reduces a voxel bitmap drawn at twice its size to one palette index per finished pixel.

constexpr int VOXEL_DOWNSAMPLE_COLORS = 256;

// The nearest colour table is indexed by a colour reduced to five bits a gun.
constexpr int VOXEL_DOWNSAMPLE_LUT_BITS = 5;
constexpr int VOXEL_DOWNSAMPLE_LUT_SIZE = 1 << (VOXEL_DOWNSAMPLE_LUT_BITS * 3);


struct VoxelDownsampleTables
{
	// Two hundred and fifty six colours of three packed guns, the layout the voxel palette
	// already has.
	unsigned char const * Palette;

	// When NULL, a block takes the commonest of its indices and no colours are mixed.
	unsigned char const * Nearest;

	// The house colour range the loaded palette library declares. Empty when RemapEnd is
	// below RemapStart.
	int RemapStart;
	int RemapEnd;
};


// Fills VOXEL_DOWNSAMPLE_LUT_SIZE entries. The transparent index and the house colour range
// are never chosen, so a mixed colour cannot turn into a house colour.
void Voxel_Build_Nearest_Table(unsigned char const * palette, int remapstart, int remapend, unsigned char * table);

// The index that stands for the two by two block at the given corner.
unsigned char Voxel_Reduce_Block(unsigned char const * block, int stride, VoxelDownsampleTables const & tables);

// Reduces a source of twice these dimensions. The destination is written in full.
void Voxel_Downsample(unsigned char const * source, int sourcestride, unsigned char * dest, int deststride, int width, int height, VoxelDownsampleTables const & tables);
