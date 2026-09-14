/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// Holds the six voxel drawers in voxlib.cpp to the output the assembly they replaced produced.
// The vectors in voxelgolden.h were recorded from that assembly before it was removed.
//
// These drawers were reached only through entries 16 to 31 of VoxelDrawFunctions, which the
// dispatch never indexed, so until that table was repointed none of this code had ever run and
// nothing would have noticed it drawing the wrong thing. Needs no game data.

#include <cstdio>
#include <cstring>
#include <climits>

#include "voxdrsys.h"
#include "voxelscale.h"
#include "voxlib.h"

#include "voxelgolden.h"

void __cdecl Draw_Voxel_Regular_Normals(VoxelFuncArgumentStruct * state);
void __cdecl Draw_Voxel_Reverse_Normals(VoxelFuncArgumentStruct * state);
void __cdecl Draw_Voxel_Regular_Normals_Lighting(VoxelFuncArgumentStruct * state);
void __cdecl Draw_Voxel_Reverse_Normals_Lighting(VoxelFuncArgumentStruct * state);
void __cdecl Draw_Voxel_Regular(VoxelFuncArgumentStruct * state);
void __cdecl Draw_Voxel_Reverse(VoxelFuncArgumentStruct * state);

/*
 * Standing in for voxdrsys.cpp, which the harness does not build. The buffers carry the
 * engine's own padding, so a drawer overrunning the bitmap is caught here rather than
 * corrupting whatever the engine happens to place next to it.
 */
extern "C" {
unsigned char VoxelDrawBuffer[VOXEL_BITMAP_BYTES];
unsigned char VoxelDrawZBuffer[VOXEL_BITMAP_BYTES];
unsigned char VoxelPaletteTranslateTable[MAX_PALETTE_LOOKUP_ENTRIES][VOXEL_PALETTE_SIZE];
}

RGBStruct VoxelRGBColors[VOXEL_PALETTE_SIZE];

namespace VoxelDrawSystem {
	BOOL EnableLighting = 0;
	BOOL EnableZBuffer = 0;
}

Matrix3D::Matrix3D(float * const) {}
VoxelPaletteLibrary::VoxelPaletteLibrary(RGBStruct *, void *) {}
VoxelPaletteLibrary::~VoxelPaletteLibrary(void) {}
void VoxelPaletteLibrary::Calculate_Lookup_Table(float *, int) {}

namespace {

// The vectors were recorded over a bitmap of this size, so the hash covers exactly this
// much however large the buffer itself is.
int const GOLDENBYTES = VOXEL_SCALE_BASE_SIZE * VOXEL_SCALE_BASE_SIZE * VOXEL_SCALE_MAX * VOXEL_SCALE_MAX;

int const SPANMAX = 4096;
int const DATAMAX = 65536;

unsigned char StartOffsets[SPANMAX];
unsigned char EndOffsets[SPANMAX];
unsigned char VoxelData[DATAMAX];

unsigned int Seed = 0;

int Failures = 0;
int Checked = 0;


unsigned int Next_Random(void)
{
	Seed = Seed * 1103515245u + 12345u;
	return(Seed >> 8);
}


unsigned long long Hash(unsigned char const * data, int size)
{
	unsigned long long hash = 1469598103934665603ULL;
	for (int i = 0; i < size; i++) {
		hash ^= (unsigned long long)data[i];
		hash *= 1099511628211ULL;
	}
	return(hash);
}

typedef void (* Drawer)(VoxelFuncArgumentStruct *);

Drawer const Drawers[6] = {
	Draw_Voxel_Regular_Normals,
	Draw_Voxel_Reverse_Normals,
	Draw_Voxel_Regular_Normals_Lighting,
	Draw_Voxel_Reverse_Normals_Lighting,
	Draw_Voxel_Regular,
	Draw_Voxel_Reverse
};

char const * const Names[6] = {
	"Draw_Voxel_Regular_Normals",
	"Draw_Voxel_Reverse_Normals",
	"Draw_Voxel_Regular_Normals_Lighting",
	"Draw_Voxel_Reverse_Normals_Lighting",
	"Draw_Voxel_Regular",
	"Draw_Voxel_Reverse"
};


/*
 * Must build byte for byte what the generator built.
 *
 * A column's spans have to account for exactly ZSize voxels between them, because the drawers
 * count down from ZSize and stop on zero; a column adding up to anything else takes the counter
 * past it and the walk runs away through the data. The column table holds 32 bit offsets and a
 * negative entry means an empty column. Each span ends with a repeat of its length, which the
 * forward drawers step over and the reverse drawers read first.
 */
void Build_Layer(unsigned int seed, int columns, int zsize, bool withnormals)
{
	Seed = seed;

	unsigned int * starts = (unsigned int *)StartOffsets;
	unsigned int * ends = (unsigned int *)EndOffsets;

	int at = 0;

	for (int i = 0; i < columns; i++) {

		if ((Next_Random() % 8) == 0) {
			starts[i] = UINT_MAX;
			ends[i] = UINT_MAX;
			continue;
		}

		starts[i] = (unsigned int)at;

		int remaining = zsize;

		while (remaining > 0) {
			int const skip = (int)(Next_Random() % (unsigned int)remaining);
			remaining -= skip;

			int const run = 1 + (int)(Next_Random() % (unsigned int)remaining);
			remaining -= run;

			VoxelData[at++] = (unsigned char)skip;
			VoxelData[at++] = (unsigned char)run;

			/*
			 * A voxel is a colour and a normal for the drawers that shade, and a colour
			 * on its own for the two that do not.
			 */
			for (int v = 0; v < run; v++) {
				VoxelData[at++] = (unsigned char)(1 + (Next_Random() % 254));

				if (withnormals) {
					VoxelData[at++] = (unsigned char)(Next_Random() % 244);
				}
			}

			VoxelData[at++] = (unsigned char)run;
		}

		ends[i] = (unsigned int)(at - 1);
	}

	for (int i = 0; i < 256 * 3; i++) {
		((unsigned char *)VoxelRGBColors)[i] = (unsigned char)(Next_Random() & 0xFF);
	}

	for (int i = 0; i < MAX_PALETTE_LOOKUP_ENTRIES; i++) {
		for (int j = 0; j < VOXEL_PALETTE_SIZE; j++) {
			VoxelPaletteTranslateTable[i][j] = (unsigned char)(Next_Random() & 0xFF);
		}
	}

	/*
	 * The shaded drawers pick a palette lookup row through this table, and the engine only
	 * ever puts a row number in it. Left zeroed it holds every voxel to row zero, which is
	 * the one case that would not tell a wrong lookup apart from a right one.
	 */
	for (int i = 0; i < VOXEL_PALETTE_SIZE; i++) {
		VoxelNormalTranslateTable[i] = (unsigned char)(Next_Random() % MAX_PALETTE_LOOKUP_ENTRIES);
	}
}


void Setup(VoxelFuncArgumentStruct & arg)
{
	std::memset(&arg, 0, sizeof(arg));

	arg.StartOffset = StartOffsets;
	arg.EndOffset = EndOffsets;
	arg.DataOffset = VoxelData;
	arg.StartIndex = 24;
	arg.StrideX = 1;
	arg.StrideY = 4;

	for (int i = 0; i < 4; i++) {
		arg.TransformMatrix[i].I = (short)(64 + i * 16);
		arg.TransformMatrix[i].J = (short)(48 + i * 8);
		arg.TransformMatrix[i].K = (short)(32 + i * 4);
	}

	arg.XSize = 4;
	arg.YSize = 4;
	arg.ZSize = 8;
}

// A projection with steps of about three pixels a voxel, so the drawn shape is large enough
// for its size to be measured. Setup's own transform draws a few pixels across.
void Setup_Spread(VoxelFuncArgumentStruct & arg, int scale)
{
	Setup(arg);

	arg.TransformMatrix[0].I = (128 * scale) << 8;
	arg.TransformMatrix[0].J = (128 * scale) << 8;
	arg.TransformMatrix[0].K = 128 << 8;

	arg.TransformMatrix[1].I = 768 * scale;
	arg.TransformMatrix[1].J = 384 * scale;
	arg.TransformMatrix[2].I = -384 * scale;
	arg.TransformMatrix[2].J = 768 * scale;
	arg.TransformMatrix[3].I = 200 * scale;
	arg.TransformMatrix[3].J = -700 * scale;
}


struct DrawnExtent
{
	int Width;
	int Height;
	int Painted;
};

DrawnExtent Measure(int stride, int rows)
{
	DrawnExtent extent = { 0, 0, 0 };

	int left = stride;
	int right = -1;
	int top = rows;
	int bottom = -1;

	for (int y = 0; y < rows; y++) {
		for (int x = 0; x < stride; x++) {
			if (VoxelDrawBuffer[y * stride + x] == 0) {
				continue;
			}
			extent.Painted++;
			if (x < left) left = x;
			if (x > right) right = x;
			if (y < top) top = y;
			if (y > bottom) bottom = y;
		}
	}

	if (right >= left) {
		extent.Width = right - left + 1;
		extent.Height = bottom - top + 1;
	}

	return(extent);
}

void Check(bool condition, char const * what)
{
	std::printf("%-52s %s\n", what, condition ? "ok" : "FAILED");
	if (!condition) {
		Failures++;
	}
	Checked++;
}

}	// namespace


int main(void)
{
	for (int i = 0; i < VoxelGoldenCaseCount; i++) {
		VoxelGoldenCase const & test = VoxelGoldenCases[i];

		Build_Layer(test.Seed, 64, 8, test.Which < 4);

		VoxelFuncArgumentStruct arg;
		std::memset(VoxelDrawBuffer, 0, sizeof(VoxelDrawBuffer));
		Setup(arg);

		Drawers[test.Which](&arg);

		unsigned long long const hash = Hash(VoxelDrawBuffer, GOLDENBYTES);

		if (hash != test.Hash) {
			std::printf("FAILED %-38s seed %u: expected %llu, got %llu\n",
				Names[test.Which], test.Seed, test.Hash, hash);
			Failures++;
		}

		Checked++;
	}

	std::printf("%-52s %s\n", "Voxel drawing matches the recorded assembly", Failures == 0 ? "ok" : "FAILED");

	/*
	 * The same model drawn at twice the scale covers twice the ground in each direction.
	 * Painted bytes grow faster than the area because a voxel covers a larger block, and
	 * they grow more slowly than that block alone would say because the spread reduces how
	 * much of it overlaps its neighbours.
	 */
	for (int which = 0; which < 6; which++) {
		Build_Layer(1234u, 64, 8, which < 4);

		VoxelFuncArgumentStruct arg;

		Set_Voxel_Scale(1);
		std::memset(VoxelDrawBuffer, 0, sizeof(VoxelDrawBuffer));
		Setup_Spread(arg, 1);
		Drawers[which](&arg);
		DrawnExtent const single = Measure(VOXEL_SCALE_BASE_SIZE, VOXEL_SCALE_BASE_SIZE);

		Set_Voxel_Scale(2);
		std::memset(VoxelDrawBuffer, 0, sizeof(VoxelDrawBuffer));
		Setup_Spread(arg, 2);
		Drawers[which](&arg);
		DrawnExtent const doubled = Measure(VOXEL_SCALE_BASE_SIZE * 2, VOXEL_SCALE_BASE_SIZE * 2);

		Set_Voxel_Scale(1);

		char label[96];

		std::snprintf(label, sizeof(label), "%.30s draws twice as wide", Names[which]);
		Check(single.Width > 8 && doubled.Width >= single.Width * 2 - 3 && doubled.Width <= single.Width * 2 + 3, label);

		std::snprintf(label, sizeof(label), "%.30s draws twice as tall", Names[which]);
		Check(single.Height > 8 && doubled.Height >= single.Height * 2 - 3 && doubled.Height <= single.Height * 2 + 3, label);

		std::snprintf(label, sizeof(label), "%.30s paints three to ten times", Names[which]);
		Check(single.Painted > 0 && doubled.Painted >= single.Painted * 3 && doubled.Painted <= single.Painted * 10, label);
	}

	/*
	 * The drawers lay a voxel's block down without a bounds test, so the buffer's padding is
	 * what keeps a voxel at the bottom edge inside it.
	 */
	bool padclean = true;
	for (int i = VOXEL_BITMAP_BYTES - 1; i >= GOLDENBYTES + VOXEL_SCALE_BASE_SIZE * VOXEL_SCALE_MAX; i--) {
		if (VoxelDrawBuffer[i] != 0) {
			padclean = false;
		}
	}
	Check(padclean, "Drawing stays within the buffer's padding");

	std::printf("checked %d cases, %d mismatches\n", Checked, Failures);

	return(Failures == 0 ? 0 : 1);
}
