/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "voxelscale.h"


namespace
{

int CurrentScale = 1;
int CurrentWidth = VOXEL_SCALE_BASE_SIZE;
int CurrentShift = VOXEL_SCALE_BASE_SHIFT;
int CurrentMask = VOXEL_SCALE_BASE_SIZE - 1;

}


void Set_Voxel_Scale(int scale)
{
	if (scale < 1) {
		scale = 1;
	}
	if (scale > VOXEL_SCALE_MAX) {
		scale = VOXEL_SCALE_MAX;
	}

	CurrentScale = scale;
	CurrentWidth = VOXEL_SCALE_BASE_SIZE * scale;

	CurrentShift = VOXEL_SCALE_BASE_SHIFT;
	for (int size = VOXEL_SCALE_BASE_SIZE; size < CurrentWidth; size *= 2) {
		CurrentShift++;
	}

	CurrentMask = CurrentWidth - 1;
}


int Voxel_Scale(void)
{
	return(CurrentScale);
}


int Voxel_Bitmap_Width(void)
{
	return(CurrentWidth);
}


int Voxel_Bitmap_Height(void)
{
	return(CurrentWidth);
}


int Voxel_Bitmap_Center(void)
{
	return(CurrentWidth / 2);
}


VoxelSpan Voxel_Span(void)
{
	VoxelSpan span;
	span.Mask = (unsigned int)CurrentMask;
	span.Shift = (unsigned int)CurrentShift;
	span.Scale = CurrentScale;
	span.Stride = CurrentWidth;
	return(span);
}
