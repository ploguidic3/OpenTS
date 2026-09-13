/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once

#include "nativewindow.hh"


// How the presented frame is filtered when the window is larger than it.
enum VideoScaleMode {
	VIDEO_SCALE_NEAREST,
	VIDEO_SCALE_LINEAR,
	VIDEO_SCALE_PIXELART,
};


// Where the game's frame lands inside the window. The frame keeps its aspect ratio, so
// the destination is centered and the window may show bars on two of its sides.
// Drawable dimensions and the destination rectangle are measured in physical pixels.
struct VideoScaleInfo
{
	int GameWidth;
	int GameHeight;
	int DrawableWidth;
	int DrawableHeight;
	int DestX;
	int DestY;
	int DestWidth;
	int DestHeight;
	float ScaleX;
	float ScaleY;
};


bool Video_Init(NativeWindow const & window, int drawablewidth, int drawableheight, int refreshrate);
void Video_Shutdown(void);

bool Video_Set_Mode(int width, int height);
void Video_On_Resize(int drawablewidth, int drawableheight);
void Video_Set_Refresh_Rate(int refreshrate);

void Video_Mark_Dirty(void);
void Video_Present(void);
void Video_Present_If_Dirty(void);

VideoScaleInfo const & Video_Get_Scale_Info(void);

// A container movie owns the window between these two calls: the game's frame is not
// presented while it does, and is marked for presenting again once it is over.
void Video_Begin_Movie(void);
void Video_End_Movie(void);

// Draws one movie frame fitted to the window's drawable area, the game's frame set
// aside. The pixels are top-down BGRA and stay owned by the caller. integerfit grows
// the frame by a whole number where it can instead of to the drawable's edge. The
// overlay, when given, is BGRA with alpha, placed at (overlayx, overlayy) of a frame
// overlayreferenceheight tall and grown with it. False when the renderer could not
// take a frame of that size.
bool Video_Present_Video_Frame(void const * bgra, int pitch, int width, int height, bool integerfit,
	void const * overlay, int overlaypitch, int overlaywidth, int overlayheight, int overlayx, int overlayy, int overlayreferenceheight);

int * EnumDisplayModes(int minwidth, int minheight, int maxwidth, int maxheight);
