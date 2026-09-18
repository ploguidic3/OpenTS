/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once


// The pixel density the tactical view projects the world at, as a whole multiple of the
// isometric tile the engine was written around.

// The tile the projection and the iso-tile rasteriser were written around.
constexpr int ASSET_TILE_BASE_W = 48;
constexpr int ASSET_TILE_BASE_H = 24;

// The largest multiple the tile tables and the expansion cache are sized for.
constexpr int ASSET_SCALE_MAX = 2;

// Clamped to one through ASSET_SCALE_MAX. Call once, before the surfaces, the tile tables
// and the voxel bitmap are built; everything derived from it is fixed for the process.
void Set_Asset_Scale(int scale);

int Asset_Scale(void);

// The scale art is magnified by for whatever is being drawn now: the asset scale while the
// tactical world is the target, and one everywhere else. The HUD, menus and dialogs are laid
// out in their own pixels and have their own settings, so the world's scale must not reach
// them.
int Draw_Scale(void);


// Makes the tactical world the draw target for as long as it is in scope.
class WorldDrawScope
{
	public:
		WorldDrawScope(void);
		~WorldDrawScope(void);

		WorldDrawScope(WorldDrawScope const &) = delete;
		WorldDrawScope & operator = (WorldDrawScope const &) = delete;

	private:
		bool Previous;
};

// A pixel quantity written for the original tile, in the scaled view's pixels. Leptons and
// cell counts are not pixel quantities and must not be passed through this.
inline int AS(int pixels)
{
	return(pixels * Asset_Scale());
}


/*
 * The depth a span of drawn pixels amounts to. The depth buffer is kept in the original
 * tile's units, so that the per-pixel depth carried by tile and shape artwork, and the
 * offsets the draw calls add to it, mean the same thing at any scale. Rounds towards
 * negative, so a row either side of the buffer's origin steps evenly.
 */
inline int Asset_Depth(int pixels)
{
	int const scale = Asset_Scale();
	if (scale <= 1) {
		return(pixels);
	}

	return((pixels >= 0) ? (pixels / scale) : -((-pixels + scale - 1) / scale));
}


// The isometric rotation, parameterised by the tile it projects onto so that it can be
// exercised away from the engine's globals. The result keeps whatever units went in.
inline void Asset_Rect_To_Iso(int xin, int yin, int tile_w, int tile_h, int & xout, int & yout)
{
	xout = xin * tile_w / 2;
	yout = xin * tile_h / 2;
	xout += yin * tile_w / -2;
	yout += yin * tile_h / 2;
}


// The determinant of the rotation above, which is what its inverse divides by. It grows with
// the square of the scale, so it cannot be written as a scaled constant.
inline int Asset_Iso_Determinant(int tile_w, int tile_h)
{
	return(tile_w * tile_h / 2);
}


// The inverse rotation. The bias matches the one the engine's integer inverse has always
// applied, so that a caller's round trip lands back where it started.
inline void Asset_Iso_To_Rect(int xin, int yin, int tile_w, int tile_h, int & xout, int & yout)
{
	int const determinant = Asset_Iso_Determinant(tile_w, tile_h);
	xout = ((tile_w / 2) * yin + (tile_h / 2) * xin) / determinant - 65536;
	yout = ((tile_w / 2) * yin - (tile_h / 2) * xin) / determinant + 65536;
}
