/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once

#include "dialog.hh"
#include "point.h"
#include "rect.h"
#include "surface.h"

class ColorScheme;
class FontClass;
class ShapeSet;


// The HUD scale in force. One until UI_Scale_Update has run.
int UI_Scale(void);

// The scale the interface artwork is drawn at: what an HD pack declares, or one for the
// classic artwork. The HUD scale is always a whole multiple of it.
int UI_Art_Scale(void);

// What the HUD surfaces are magnified by on their way into the frame.
int UI_Present_Scale(void);

// A layout distance in artwork pixels rather than in the classic pixels it is written in.
int UI_Art(int value);

// The shape as the interface draws it: enlarged when the pack left this one at the classic
// size, so a pack that replaces part of the interface still lays out as one picture.
ShapeSet const * UI_Art_Shape(ShapeSet const * shape);

// What text drawn in this font is magnified by to reach the frame.
int UI_Text_Factor(FontClass const * font);

// How far text has to be magnified to match the artwork on the HUD's own surfaces, which a
// pack at the artwork's scale leaves at one.
int UI_Art_Text_Factor(FontClass const * font);

// Fancy_Text_Print into a HUD surface, magnified to the artwork around it. The point means
// what it does there, alignment included.
Point2D UI_Art_Text_Print(char const * text, Surface & surface, Rect const & rect, Point2D const & point,
	ColorScheme * fore, int back, TextPrintType style);
Point2D UI_Art_Text_Print(int text, Surface & surface, Rect const & rect, Point2D const & point,
	ColorScheme * fore, int back, TextPrintType style);

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

// Conversions between sidebar surface pixels, which are artwork pixels, and frame pixels.
// A layout constant written in classic pixels passes through UI_Art on the way in.
Rect Sidebar_To_Frame(Rect const & rect);
Point2D Sidebar_To_Frame(Point2D const & point);
Point2D Frame_To_Sidebar(Point2D const & point);

// The surface the tab strip is drawn into at its own size, sized to the composite surface.
Surface * UI_Tab_Surface(void);

// Copies the tab strip, or one region of it, magnified into the composite and tile surfaces.
void UI_Present_Tab_Strip(void);
void UI_Present_Tab_Strip(Rect const & region);

// Scratch surface of at least the given size for drawing at the HUD's own scale. The
// contents are cleared to UI_SCRATCH_KEY; the surface belongs to this module.
Surface * UI_Scratch_Surface(int width, int height);

// Pixels the scratch is cleared to. Fonts never produce this pure magenta, so it can stand
// for an untouched pixel where zero cannot: black is a real pixel in a text shadow.
constexpr unsigned short UI_SCRATCH_KEY = 0xF81F;

// Copies a scratch drawing of the given size into the destination, each pixel grown to a
// square of the given factor. Pixels still holding UI_SCRATCH_KEY are skipped when transparent is set.
void UI_Scratch_Present(Surface & dest, Point2D const & at, int width, int height, bool transparent, int factor);


// Draws through the callable at the HUD's own scale and lands the result magnified at a
// frame position; the callable receives the surface and the content's top left corner.
template<class Draw>
void UI_Draw_Scaled(Surface & dest, Point2D const & at, int width, int height, bool transparent, int factor, Draw && draw)
{
	if (factor <= 1) {
		draw(dest, at);
		return;
	}

	Surface * scratch = UI_Scratch_Surface(width, height);
	if (scratch == nullptr) {
		draw(dest, at);
		return;
	}

	draw(*scratch, Point2D(0, 0));
	UI_Scratch_Present(dest, at, width, height, transparent, factor);
}
