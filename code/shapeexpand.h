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

// Drops every held frame. Call when the shapes themselves are released.
void Shape_Expansion_Reset(void);

unsigned Shape_Expansion_Bytes(void);
