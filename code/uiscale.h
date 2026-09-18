/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once


// Relates HUD pixels, drawn at the 640x480-era size, to the frame they are magnified into.

// The HUD's own pixel sizes. The sidebar surface is this wide and the tab strip this tall.
constexpr int UI_SIDEBAR_WIDTH = 168;
constexpr int UI_TAB_HEIGHT = 16;

// Bounds for the setting and for the layout it produces.
constexpr int UI_SCALE_MAX = 4;
constexpr int UI_SCALE_MIN_SIDEBAR_HEIGHT = 400;
constexpr int UI_SCALE_MIN_TACTICAL_WIDTH = 320;
constexpr int UI_SCALE_AUTO_REFERENCE_HEIGHT = 720;


struct UIPoint
{
	int X;
	int Y;

	constexpr bool operator == (UIPoint const & rvalue) const = default;
};


struct UIBox
{
	int X;
	int Y;
	int Width;
	int Height;

	constexpr bool operator == (UIBox const & rvalue) const = default;
};


// The scale the frame height asks for on its own: one at 720 lines, one more for every
// further 720, rounded to nearest.
int UI_Scale_Auto(int height);

// Whether a scaled HUD leaves a sidebar of usable height and a tactical view of usable width.
bool UI_Scale_Fits(int width, int height, int scale);

// Resolves the setting: zero picks the automatic value, one to four are honoured, anything
// else is clamped, and the result is reduced until it fits the frame. Never below one.
int UI_Scale_For(int setting, int width, int height);

// Division that rounds toward negative infinity, so a point just left of the HUD origin does
// not fold back onto its first column.
int UI_Floor_Div(int value, int divisor);

// The scale the interface artwork is drawn at. A pack declares it; it is honoured only when
// the HUD scale is a whole multiple of it, since every offset and hit test would otherwise
// land between pixels. One means the classic artwork, magnified by the HUD scale as before.
int UI_Art_Scale_For(int pack, int scale);

// The HUD scale reduced to a whole multiple of the art scale. A 2x pack at a screen asking
// for three therefore draws its artwork at two rather than at one and a half.
int UI_Scale_For_Art(int scale, int art);

// How far the surfaces the HUD is drawn into are magnified on their way to the frame. One
// when the artwork is already at the HUD scale.
int UI_Present_Scale_For(int scale, int art);

int UI_Sidebar_Surface_Height(int frame_height, int present);
int UI_Tab_Surface_Width(int composite_width, int present);

// The frame rectangle a HUD-space rectangle occupies once magnified about the given origin.
UIBox UI_HUD_To_Frame(UIBox const & box, UIPoint const & origin, int scale);
UIPoint UI_HUD_To_Frame(UIPoint const & point, UIPoint const & origin, int scale);

// The HUD-space pixel under a frame position.
UIPoint UI_Frame_To_HUD(UIPoint const & point, UIPoint const & origin, int scale);
