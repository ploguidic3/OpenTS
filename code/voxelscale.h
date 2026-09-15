/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once


// The size the voxel rasteriser draws at, as a whole multiple of its original bitmap.

// The bitmap the rasteriser was written around, and the shift that names its row stride.
constexpr int VOXEL_SCALE_BASE_SIZE = 256;
constexpr int VOXEL_SCALE_BASE_SHIFT = 8;

// The largest multiple the 8.8 projection and the cache arena are sized for.
constexpr int VOXEL_SCALE_MAX = 2;

// The widest bitmap any of the above can ask for. Buffers that must be fixed use this.
constexpr int VOXEL_SCALE_MAX_SIZE = VOXEL_SCALE_BASE_SIZE * VOXEL_SCALE_MAX;

// A run length in the voxel file is one byte, so a skip table has this many entries
// whatever the bitmap measures.
constexpr int VOXEL_MAX_RUN = 256;

// The bitmap buffers carry this much slack past their last row, because a voxel at the
// bottom edge lays down a block that reaches below and to the right of it.
constexpr int VOXEL_BITMAP_PAD = VOXEL_SCALE_MAX_SIZE * VOXEL_SCALE_MAX + 2 * VOXEL_SCALE_MAX;
constexpr int VOXEL_BITMAP_BYTES = VOXEL_SCALE_MAX_SIZE * VOXEL_SCALE_MAX_SIZE + VOXEL_BITMAP_PAD;

// The cache arena the rasteriser was written around. A pose covers the square of the scale,
// so the arena has to grow by that much to hold as many of them as it did before.
constexpr int VOXEL_CACHE_BASE_BYTES = 2000000;

// Clamped to one through VOXEL_SCALE_MAX. Call once, before the voxel buffers are allocated.
void Set_Voxel_Scale(int scale);

int Voxel_Scale(void);
int Voxel_Bitmap_Width(void);
int Voxel_Bitmap_Height(void);

// Half the bitmap, which is the bias that centres an object in it.
int Voxel_Bitmap_Center(void);


// The bitmap geometry a drawer needs, read once before it walks an object. Mask keeps a
// coordinate inside the bitmap and Shift is its row stride.
struct VoxelSpan
{
	unsigned int Mask;
	unsigned int Shift;
	int Scale;
	int Stride;
};

VoxelSpan Voxel_Span(void);


// Where a drawn object sits in the bitmap, and the offset to blit it at. X, Y, Width and
// Height are the rectangle to take out of the bitmap; PointX and PointY are added to the
// object's draw position on screen.
struct VoxelRegion
{
	int X;
	int Y;
	int Width;
	int Height;
	int PointX;
	int PointY;
};


// Width and height are the object's extent in the bitmap and center its midpoint, both in
// bitmap pixels. The slack left around the object grows with the scale, so a scaled object
// keeps the same margin in the terms the caller measures it in.
VoxelRegion Voxel_Region(int width, int height, int center_x, int center_y);


// At mask 0xFF and shift 8 this is the drawers' original expression, wrap included.
inline unsigned int Voxel_Buffer_Index(unsigned int pixel_x, unsigned int pixel_y, unsigned int mask, unsigned int shift)
{
	return(((pixel_x >> 8) & mask) + (((pixel_y >> 8) & mask) << shift));
}
