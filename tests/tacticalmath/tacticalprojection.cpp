/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// Exercises the tactical projection primitives on their own, at both asset scales. Needs no
// game data and no engine.

#include <cstdio>

#include "assetscale.h"

namespace {

int Failures = 0;


void Check(bool condition, char const * what)
{
	std::printf("%-62s %s\n", what, condition ? "ok" : "FAILED");

	if (!condition) {
		Failures++;
	}
}


int Tile_W(void)
{
	return(ASSET_TILE_BASE_W * Asset_Scale());
}


int Tile_H(void)
{
	return(ASSET_TILE_BASE_H * Asset_Scale());
}


// The inverse carries the bias the engine's integer form has always applied, so a round trip
// lands one map width away on each axis rather than exactly where it started.
void Check_Round_Trip(int x, int y, char const * what)
{
	int isox, isoy;
	Asset_Rect_To_Iso(x, y, Tile_W(), Tile_H(), isox, isoy);

	int backx, backy;
	Asset_Iso_To_Rect(isox, isoy, Tile_W(), Tile_H(), backx, backy);

	Check(backx == x - 65536 && backy == y + 65536, what);
}

}


int main(void)
{
	std::printf("Tactical projection\n\n");

	Set_Asset_Scale(1);
	Check(Asset_Scale() == 1, "Scale one is the default and is honoured");
	Check(Tile_W() == 48 && Tile_H() == 24, "Scale one keeps the original 48x24 tile");
	Check(Asset_Iso_Determinant(Tile_W(), Tile_H()) == 576, "Scale one inverts through 576");
	Check(AS(12) == 12, "AS leaves a pixel quantity alone at scale one");

	Check_Round_Trip(0, 0, "Scale one round trips the origin");
	Check_Round_Trip(1, 0, "Scale one round trips one cell east");
	Check_Round_Trip(0, 1, "Scale one round trips one cell south");
	Check_Round_Trip(37, 61, "Scale one round trips an interior cell");
	Check_Round_Trip(511, 511, "Scale one round trips the far map corner");
	Check_Round_Trip(-40, 25, "Scale one round trips a negative coordinate");

	int basex, basey;
	Asset_Rect_To_Iso(37, 61, Tile_W(), Tile_H(), basex, basey);

	Set_Asset_Scale(2);
	Check(Asset_Scale() == 2, "Scale two is honoured");
	Check(Tile_W() == 96 && Tile_H() == 48, "Scale two doubles the tile to 96x48");
	Check(AS(12) == 24, "AS doubles a pixel quantity at scale two");

	// The determinant is quadratic in the scale, which is why it cannot be written as a
	// scaled constant.
	Check(Asset_Iso_Determinant(Tile_W(), Tile_H()) == 2304, "Scale two inverts through 2304");

	Check_Round_Trip(0, 0, "Scale two round trips the origin");
	Check_Round_Trip(1, 0, "Scale two round trips one cell east");
	Check_Round_Trip(0, 1, "Scale two round trips one cell south");
	Check_Round_Trip(37, 61, "Scale two round trips an interior cell");
	Check_Round_Trip(511, 511, "Scale two round trips the far map corner");
	Check_Round_Trip(-40, 25, "Scale two round trips a negative coordinate");

	int scaledx, scaledy;
	Asset_Rect_To_Iso(37, 61, Tile_W(), Tile_H(), scaledx, scaledy);
	Check(scaledx == basex * 2 && scaledy == basey * 2, "Scale two projects to exactly twice scale one");

	/*
	 * The scale belongs to the world only. The HUD, the menus and the dialogs are laid out in
	 * their own pixels, so art drawn outside a world scope must come back unmagnified.
	 */
	Check(Draw_Scale() == 1, "Art drawn outside the world is not magnified");
	{
		WorldDrawScope const worldscope;
		Check(Draw_Scale() == 2, "Art drawn in the world takes the asset scale");
		{
			WorldDrawScope const nested;
			Check(Draw_Scale() == 2, "A nested world scope stays in the world");
		}
		Check(Draw_Scale() == 2, "Leaving a nested scope stays in the world");
	}
	Check(Draw_Scale() == 1, "Leaving the world scope restores the unmagnified draw");

	Set_Asset_Scale(1);
	{
		WorldDrawScope const worldscope;
		Check(Draw_Scale() == 1, "The world draws unmagnified at scale one");
	}

	/*
	 * The depth buffer is kept in the original tile's units, so that the per-pixel depth that
	 * tile and shape artwork carries keeps its meaning however many pixels a row covers.
	 */
	Set_Asset_Scale(1);
	Check(Asset_Depth(37) == 37 && Asset_Depth(-37) == -37, "Scale one leaves a depth unchanged");

	Set_Asset_Scale(2);
	Check(Asset_Depth(0) == 0, "The buffer origin is the same depth at either scale");
	Check(Asset_Depth(2) == 1, "Two drawn rows are one row of the original tile");
	Check(Asset_Depth(3) == 1, "An odd span rounds down");
	Check(Asset_Depth(-2) == -1 && Asset_Depth(-3) == -2, "A span above the origin rounds towards negative");

	bool evenstep = true;
	for (int row = -20; row < 20; row++) {
		if (Asset_Depth(row + 2) - Asset_Depth(row) != 1) {
			evenstep = false;
		}
	}
	Check(evenstep, "Every two drawn rows step the depth by exactly one");

	Set_Asset_Scale(0);
	Check(Asset_Scale() == 1, "A scale below one is clamped up");

	Set_Asset_Scale(9);
	Check(Asset_Scale() == ASSET_SCALE_MAX, "A scale past the maximum is clamped down");

	std::printf("\n%d failure(s)\n", Failures);
	return(Failures == 0 ? 0 : 1);
}
