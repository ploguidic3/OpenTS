/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once

class ShapeSet;


// Frames of classic art magnified to the asset scale, held so that a sprite redrawn every
// tick is expanded once rather than every time.

// The most the held frames are allowed to occupy before the least recently drawn are dropped.
constexpr unsigned SHAPE_EXPAND_BUDGET = 64u * 1024u * 1024u;


// The two most recently returned frames are never evicted, so one draw can hold a shape and
// its depth shape at once.
struct ExpandedFrame
{
	unsigned char const * Data = nullptr;
	int Width = 0;
	int Height = 0;
};


// The frame magnified by factor and uncompressed, or an empty frame if it cannot be produced.
ExpandedFrame Shape_Expanded_Frame(ShapeSet const * shapefile, int shapenum, int factor);

// The same frame re-encoded in the row format the shape files carry. Width and Height still
// describe the magnified frame; only Data is encoded. The RLE blitter is the only one that
// can take a depth shape, so a shape drawn with one has to reach it in this form.
ExpandedFrame Shape_Expanded_RLE_Frame(ShapeSet const * shapefile, int shapenum, int factor);

// How far Draw_Shape magnifies this shape for the current draw target. A caller that records
// the rectangle a draw covered has to apply this to match what was drawn.
int Shape_Draw_Factor(ShapeSet const * shapefile);

// How far this shape is magnified when the world is drawn, whichever target is current. Code
// measuring the rectangle a world draw will cover uses this, since it may run before the draw.
int Shape_World_Factor(ShapeSet const * shapefile);

// Drops every held frame. Call when the shapes themselves are released.
void Shape_Expansion_Reset(void);

unsigned Shape_Expansion_Bytes(void);
