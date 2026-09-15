/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once


// The geometry of the isometric tile diamond, generated for a scale rather than spelled out.

// The tile the rasteriser was written around: rows of the mask, its width, and the packed
// size of one tile's pixels.
constexpr int ISO_TILE_BASE_ROWS = 24;
constexpr int ISO_TILE_BASE_WIDTH = 48;
constexpr int ISO_TILE_BASE_PIXELS = 576;

// The characters the inherited mask tables are spelled with, which the span builder compares
// against directly.
constexpr unsigned char ISO_TILE_OUTSIDE = 0x20;
constexpr unsigned char ISO_TILE_INSIDE = 0xDB;


// The packed length of one diamond row, zero for a row past the diamond.
int Iso_Row_Span(int row, int scale);

// Cumulative offset of each row into the packed tile, for ISO_TILE_BASE_ROWS * scale entries.
void Iso_Build_Row_Bases(int scale, int * bases);

// The diamond mask, ISO_TILE_BASE_WIDTH * scale by ISO_TILE_BASE_ROWS * scale bytes. Rows is
// how far down the diamond starts, which one inherited copy of the mask is spelled with.
void Iso_Build_Mask(int scale, int row_offset, unsigned char * mask);

// Magnifies packed diamond tile data, pixels or depth alike. Source holds ISO_TILE_BASE_PIXELS
// bytes and dest holds that times the square of the scale.
void Iso_Expand_Tile(unsigned char const * source, unsigned char * dest, int scale);
