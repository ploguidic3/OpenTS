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
#include "goptions.h"
#include "options.h"
#include "sidebar.h"
#include "uiscale.h"

#include <algorithm>


static_assert(UI_SIDEBAR_WIDTH == SidebarClass::SIDE_WIDTH);

static int _Scale = 1;
static DSurface * _TabSurface = nullptr;
static DSurface * _Scratch = nullptr;


int UI_Scale(void)
{
	return(_Scale);
}


void UI_Scale_Update(void)
{
	int wanted = UI_Scale_For(Options.UIScale, VisibleRect.Width, VisibleRect.Height);
	if (wanted != _Scale) {
		DebugString("UIScale %d resolves to %d at %dx%d\n", Options.UIScale, wanted, VisibleRect.Width, VisibleRect.Height);
	}
	_Scale = wanted;
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
	return(Rect(0, 0, UI_SIDEBAR_WIDTH, UI_Sidebar_Surface_Height(VisibleRect.Height, _Scale)));
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
	UIBox box = UI_HUD_To_Frame(UIBox{rect.X, rect.Y, rect.Width, rect.Height}, To_UI(UI_Sidebar_Origin()), _Scale);
	return(Rect(box.X, box.Y, box.Width, box.Height));
}


Point2D Sidebar_To_Frame(Point2D const & point)
{
	UIPoint result = UI_HUD_To_Frame(To_UI(point), To_UI(UI_Sidebar_Origin()), _Scale);
	return(Point2D(result.X, result.Y));
}


Point2D Frame_To_Sidebar(Point2D const & point)
{
	UIPoint result = UI_Frame_To_HUD(To_UI(point), To_UI(UI_Sidebar_Origin()), _Scale);
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
	return(Sized_Surface(_TabSurface, UI_Tab_Surface_Width(CompositeSurface->Get_Width(), _Scale), UI_TAB_HEIGHT, true));
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
	Rect dest(source.X * _Scale, source.Y * _Scale, source.Width * _Scale, source.Height * _Scale);

	// The remainder the scale leaves at the right edge is never covered by the strip.
	Rect remainder(tab->Get_Width() * _Scale, 0, CompositeSurface->Get_Width() - tab->Get_Width() * _Scale, UI_TAB_HEIGHT * _Scale);

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


void UI_Scratch_Present(Surface & dest, Point2D const & at, int width, int height, bool transparent)
{
	if (_Scratch == nullptr || width <= 0 || height <= 0) {
		return;
	}
	if (dest.Bytes_Per_Pixel() != 2 || _Scratch->Bytes_Per_Pixel() != 2) {
		return;
	}

	Rect target = Intersect(Rect(at.X, at.Y, width * _Scale, height * _Scale), dest.Get_Rect());
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
		unsigned short const * row = (unsigned short const *)(source + ((y - at.Y) / _Scale) * source_stride);
		unsigned short * out = (unsigned short *)(output + y * dest_stride) + target.X;

		for (int x = target.X; x < target.X + target.Width; x++, out++) {
			unsigned short pixel = row[(x - at.X) / _Scale];
			if (!transparent || pixel != UI_SCRATCH_KEY) {
				*out = pixel;
			}
		}
	}

	dest.Unlock();
	_Scratch->Unlock();
}
