/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once

#include "point.h"
#include "rect.h"
#include "surface.h"


// The HUD scale in force. One until UI_Scale_Update has run, so menus never see it.
int UI_Scale(void);

// Resolves the HUD scale from the video settings. Call it before the surfaces are
// allocated for a resolution, since the layout below follows it.
void UI_Scale_Update(void);

// Frame-space sizes of the magnified HUD parts.
int UI_Sidebar_Frame_Width(void);
int UI_Tab_Frame_Height(void);

// The sidebar surface to allocate: the HUD's own width and the frame height reduced by
// the scale, so the magnified copy fills the frame.
Rect UI_Sidebar_Surface_Rect(void);

// Where the sidebar's top left corner lands in the frame.
Point2D UI_Sidebar_Origin(void);

// Conversions between sidebar surface pixels and frame pixels.
Rect Sidebar_To_Frame(Rect const & rect);
Point2D Sidebar_To_Frame(Point2D const & point);
Point2D Frame_To_Sidebar(Point2D const & point);

// The surface the tab strip is drawn into at its own size, sized to the composite surface.
Surface * UI_Tab_Surface(void);

// Copies the tab strip, magnified, into the composite and tile surfaces.
void UI_Present_Tab_Strip(void);

// Scratch surface of at least the given size for drawing at the HUD's own scale. The
// contents are cleared to the transparent pixel; the surface belongs to this module.
Surface * UI_Scratch_Surface(int width, int height);

// Copies a scratch drawing of the given size into the destination, each pixel grown to a
// square of the HUD scale. Pixels of value zero are skipped when transparent is set.
void UI_Scratch_Present(Surface & dest, Point2D const & at, int width, int height, bool transparent);


// Draws through the callable at the HUD's own scale and lands the result magnified at a
// frame position; the callable receives the surface and the content's top left corner.
template<class Draw>
void UI_Draw_Scaled(Surface & dest, Point2D const & at, int width, int height, bool transparent, Draw && draw)
{
	if (UI_Scale() == 1) {
		draw(dest, at);
		return;
	}

	Surface * scratch = UI_Scratch_Surface(width, height);
	if (scratch == nullptr) {
		draw(dest, at);
		return;
	}

	draw(*scratch, Point2D(0, 0));
	UI_Scratch_Present(dest, at, width, height, transparent);
}
