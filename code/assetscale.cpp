/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "assetscale.h"


namespace
{

int CurrentScale = 1;
bool DrawingWorld = false;

}


void Set_Asset_Scale(int scale)
{
	if (scale < 1) {
		scale = 1;
	}
	if (scale > ASSET_SCALE_MAX) {
		scale = ASSET_SCALE_MAX;
	}

	CurrentScale = scale;
}


int Asset_Scale(void)
{
	return(CurrentScale);
}


int Draw_Scale(void)
{
	return(DrawingWorld ? CurrentScale : 1);
}


WorldDrawScope::WorldDrawScope(void) :
	Previous(DrawingWorld)
{
	DrawingWorld = true;
}


WorldDrawScope::~WorldDrawScope(void)
{
	DrawingWorld = Previous;
}
