/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "uiscale.h"


int UI_Scale_Auto(int height)
{
	if (height <= 0) {
		return(1);
	}

	int scale = (height + UI_SCALE_AUTO_REFERENCE_HEIGHT / 2) / UI_SCALE_AUTO_REFERENCE_HEIGHT;
	if (scale < 1) scale = 1;
	if (scale > UI_SCALE_MAX) scale = UI_SCALE_MAX;
	return(scale);
}


bool UI_Scale_Fits(int width, int height, int scale)
{
	if (scale < 1) {
		return(false);
	}
	return(height / scale >= UI_SCALE_MIN_SIDEBAR_HEIGHT && width - UI_SIDEBAR_WIDTH * scale >= UI_SCALE_MIN_TACTICAL_WIDTH);
}


int UI_Scale_For(int setting, int width, int height)
{
	int scale = setting;

	if (scale <= 0) {
		scale = UI_Scale_Auto(height);
	}
	if (scale > UI_SCALE_MAX) {
		scale = UI_SCALE_MAX;
	}

	while (scale > 1 && !UI_Scale_Fits(width, height, scale)) {
		scale--;
	}
	return(scale);
}


int UI_Floor_Div(int value, int divisor)
{
	if (divisor <= 1) {
		return(value);
	}

	int quotient = value / divisor;
	if (value < 0 && quotient * divisor != value) {
		quotient--;
	}
	return(quotient);
}


int UI_Sidebar_Surface_Height(int frame_height, int scale)
{
	return(UI_Floor_Div(frame_height, scale));
}


int UI_Tab_Surface_Width(int composite_width, int scale)
{
	return(UI_Floor_Div(composite_width, scale));
}


UIBox UI_HUD_To_Frame(UIBox const & box, UIPoint const & origin, int scale)
{
	return(UIBox{origin.X + box.X * scale, origin.Y + box.Y * scale, box.Width * scale, box.Height * scale});
}


UIPoint UI_HUD_To_Frame(UIPoint const & point, UIPoint const & origin, int scale)
{
	return(UIPoint{origin.X + point.X * scale, origin.Y + point.Y * scale});
}


UIPoint UI_Frame_To_HUD(UIPoint const & point, UIPoint const & origin, int scale)
{
	return(UIPoint{UI_Floor_Div(point.X - origin.X, scale), UI_Floor_Div(point.Y - origin.Y, scale)});
}
