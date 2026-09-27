/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "always.h"

#include "uilayout.h"

#include "_rect.h"
#include "_surface.h"
#include "dbgprint.h"
#include "dsurface.h"
#include "font.h"
#include "goptions.h"
#include "options.h"
#include "shapehd.h"
#include "shapeload.h"
#include "sidebar.h"
#include "uiscale.h"

#include <algorithm>


static_assert(UI_SIDEBAR_WIDTH == SidebarClass::SIDE_WIDTH);

static int _Scale = 1;
static int _ArtScale = 1;
static int _PresentScale = 1;
static DSurface * _TabSurface = nullptr;
static DSurface * _Scratch = nullptr;


int UI_Scale(void)
{
	return(_Scale);
}


int UI_Art_Scale(void)
{
	return(_ArtScale);
}


int UI_Present_Scale(void)
{
	return(_PresentScale);
}


int UI_Art(int value)
{
	return(value * _ArtScale);
}


ShapeSet const * UI_Art_Shape(ShapeSet const * shape)
{
	if (shape == nullptr || _ArtScale == 1) {
		return(shape);
	}

	int const scale = Shape_Scale(shape);
	return(scale >= _ArtScale ? shape : Scaled_Shape(shape, _ArtScale / scale));
}


int UI_Text_Factor(FontClass const * font)
{
	int const scale = font != nullptr ? font->Get_Scale() : 1;
	int const factor = _Scale / (scale > 0 ? scale : 1);

	return(factor < 1 ? 1 : factor);
}


void UI_Scale_Update(void)
{
	int wanted = UI_Scale_For(Options.UIScale, VisibleRect.Width, VisibleRect.Height);
	int art = UI_Art_Scale_For(Pack_UI_Scale(), wanted);
	wanted = UI_Scale_For_Art(wanted, art);

	if (wanted != _Scale || art != _ArtScale) {
		DebugString("UIScale %d resolves to %d at %dx%d, artwork at %d\n", Options.UIScale, wanted, VisibleRect.Width, VisibleRect.Height, art);
	}

	_Scale = wanted;
	_ArtScale = art;
	_PresentScale = UI_Present_Scale_For(wanted, art);
}


int UI_Sidebar_Frame_Width(void)
{
	return(UI_SIDEBAR_WIDTH * _Scale);
}


int UI_Tab_Frame_Height(void)
{
	return(UI_TAB_HEIGHT * _Scale);
}


Rect UI_Sidebar_Surface_Rect(void)
{
	return(Rect(0, 0, UI_SIDEBAR_WIDTH * _ArtScale, UI_Sidebar_Surface_Height(VisibleRect.Height, _PresentScale)));
}


Point2D UI_Sidebar_Origin(void)
{
	return(Point2D(Options.IsSidebarOnRight ? TacticalRect.X + TacticalRect.Width : 0, 0));
}


static UIPoint To_UI(Point2D const & point)
{
	return(UIPoint{point.X, point.Y});
}


Rect Sidebar_To_Frame(Rect const & rect)
{
	UIBox box = UI_HUD_To_Frame(UIBox{rect.X, rect.Y, rect.Width, rect.Height}, To_UI(UI_Sidebar_Origin()), _PresentScale);
	return(Rect(box.X, box.Y, box.Width, box.Height));
}


Point2D Sidebar_To_Frame(Point2D const & point)
{
	UIPoint result = UI_HUD_To_Frame(To_UI(point), To_UI(UI_Sidebar_Origin()), _PresentScale);
	return(Point2D(result.X, result.Y));
}


Point2D Frame_To_Sidebar(Point2D const & point)
{
	UIPoint result = UI_Frame_To_HUD(To_UI(point), To_UI(UI_Sidebar_Origin()), _PresentScale);
	return(Point2D(result.X, result.Y));
}


static DSurface * Sized_Surface(DSurface *& surface, int width, int height, bool exact)
{
	if (width <= 0 || height <= 0) {
		return(nullptr);
	}

	bool fits = surface != nullptr && (exact
		? (surface->Get_Width() == width && surface->Get_Height() == height)
		: (surface->Get_Width() >= width && surface->Get_Height() >= height));

	if (!fits) {
		delete surface;
		surface = new DSurface(width, height);
		surface->Fill(0);
	}
	return(surface);
}


Surface * UI_Tab_Surface(void)
{
	if (CompositeSurface == nullptr) {
		return(nullptr);
	}
	return(Sized_Surface(_TabSurface, UI_Tab_Surface_Width(CompositeSurface->Get_Width(), _PresentScale), UI_TAB_HEIGHT * _ArtScale, true));
}


void UI_Present_Tab_Strip(void)
{
	Surface * tab = UI_Tab_Surface();
	if (tab != nullptr) {
		UI_Present_Tab_Strip(tab->Get_Rect());
	}
}


void UI_Present_Tab_Strip(Rect const & region)
{
	Surface * tab = UI_Tab_Surface();
	if (tab == nullptr) {
		return;
	}

	Rect source = Intersect(region, tab->Get_Rect());
	if (!source.Is_Valid()) {
		return;
	}
	Rect dest(source.X * _PresentScale, source.Y * _PresentScale, source.Width * _PresentScale, source.Height * _PresentScale);

	// The remainder the scale leaves at the right edge is never covered by the strip.
	Rect remainder(tab->Get_Width() * _PresentScale, 0, CompositeSurface->Get_Width() - tab->Get_Width() * _PresentScale, UI_Tab_Frame_Height());

	Surface * targets[2] = {CompositeSurface, TileSurface};
	for (Surface * target : targets) {
		if (target == nullptr) continue;
		target->Blit_From(dest, *tab, source, false, true);
		if (remainder.Is_Valid()) {
			target->Fill_Rect(remainder, 0);
		}
	}
}


Surface * UI_Scratch_Surface(int width, int height)
{
	DSurface * scratch = Sized_Surface(_Scratch, width, height, false);
	if (scratch != nullptr) {
		scratch->Fill_Rect(Rect(0, 0, width, height), UI_SCRATCH_KEY);
	}
	return(scratch);
}


void UI_Scratch_Present(Surface & dest, Point2D const & at, int width, int height, bool transparent, int factor)
{
	if (_Scratch == nullptr || width <= 0 || height <= 0) {
		return;
	}
	if (dest.Bytes_Per_Pixel() != 2 || _Scratch->Bytes_Per_Pixel() != 2) {
		return;
	}

	if (factor < 1) {
		factor = 1;
	}

	Rect target = Intersect(Rect(at.X, at.Y, width * factor, height * factor), dest.Get_Rect());
	if (!target.Is_Valid()) {
		return;
	}

	unsigned char const * source = (unsigned char const *)_Scratch->Lock();
	unsigned char * output = (unsigned char *)dest.Lock();
	if (source == nullptr || output == nullptr) {
		if (source != nullptr) _Scratch->Unlock();
		if (output != nullptr) dest.Unlock();
		return;
	}

	int source_stride = _Scratch->Stride();
	int dest_stride = dest.Stride();

	for (int y = target.Y; y < target.Y + target.Height; y++) {
		unsigned short const * row = (unsigned short const *)(source + ((y - at.Y) / factor) * source_stride);
		unsigned short * out = (unsigned short *)(output + y * dest_stride) + target.X;

		for (int x = target.X; x < target.X + target.Width; x++, out++) {
			unsigned short pixel = row[(x - at.X) / factor];
			if (!transparent || pixel != UI_SCRATCH_KEY) {
				*out = pixel;
			}
		}
	}

	dest.Unlock();
	_Scratch->Unlock();
}
