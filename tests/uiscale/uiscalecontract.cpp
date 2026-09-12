/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// Exercises the HUD scale arithmetic without the engine: the automatic rule, the fit
// reduction, and the mapping between HUD pixels and frame pixels on either sidebar side.

#include <cstdio>

#include "uiscale.h"

namespace {

int Failures = 0;


void Check(bool condition, char const * what)
{
	std::printf("%-62s %s\n", what, condition ? "ok" : "FAILED");

	if (!condition) {
		Failures++;
	}
}


void Check_Scale(int setting, int width, int height, int expected, char const * what)
{
	int actual = UI_Scale_For(setting, width, height);
	Check(actual == expected, what);
	if (actual != expected) {
		std::printf("    got %d, expected %d\n", actual, expected);
	}
}


void Check_Box(UIBox const & actual, UIBox const & expected, char const * what)
{
	Check(actual == expected, what);
	if (!(actual == expected)) {
		std::printf("    got (%d,%d,%d,%d), expected (%d,%d,%d,%d)\n",
			actual.X, actual.Y, actual.Width, actual.Height,
			expected.X, expected.Y, expected.Width, expected.Height);
	}
}


void Check_Point(UIPoint const & actual, UIPoint const & expected, char const * what)
{
	Check(actual == expected, what);
	if (!(actual == expected)) {
		std::printf("    got (%d,%d), expected (%d,%d)\n", actual.X, actual.Y, expected.X, expected.Y);
	}
}


void Test_Auto_Rule(void)
{
	Check(UI_Scale_Auto(480) == 1, "auto: 480 lines is 1");
	Check(UI_Scale_Auto(720) == 1, "auto: 720 lines is 1");
	Check(UI_Scale_Auto(768) == 1, "auto: 768 lines is 1");
	Check(UI_Scale_Auto(900) == 1, "auto: 900 lines is 1");
	Check(UI_Scale_Auto(1080) == 2, "auto: 1080 lines rounds up to 2");
	Check(UI_Scale_Auto(1200) == 2, "auto: 1200 lines is 2");
	Check(UI_Scale_Auto(1440) == 2, "auto: 1440 lines is 2");
	Check(UI_Scale_Auto(1600) == 2, "auto: 1600 lines is 2");
	Check(UI_Scale_Auto(2160) == 3, "auto: 2160 lines is 3");
	Check(UI_Scale_Auto(4320) == 4, "auto: 4320 lines caps at 4");
	Check(UI_Scale_Auto(0) == 1, "auto: no height is 1");
}


void Test_Setting(void)
{
	Check_Scale(0, 1920, 1080, 2, "setting 0 at 1080p is auto 2");
	Check_Scale(0, 2560, 1440, 2, "setting 0 at 1440p is auto 2");
	Check_Scale(0, 3840, 2160, 3, "setting 0 at 4K is auto 3");
	Check_Scale(0, 1280, 720, 1, "setting 0 at 720p is auto 1");
	Check_Scale(1, 3840, 2160, 1, "setting 1 is honoured at 4K");
	Check_Scale(2, 3840, 2160, 2, "setting 2 is honoured at 4K");
	Check_Scale(3, 3840, 2160, 3, "setting 3 is honoured at 4K");
	Check_Scale(4, 3840, 2160, 4, "setting 4 is honoured at 4K");
	Check_Scale(7, 3840, 2160, 4, "setting above 4 clamps to 4");
	Check_Scale(-3, 1920, 1080, 2, "negative setting behaves as auto");
}


void Test_Fit(void)
{
	Check(UI_Scale_Fits(640, 400, 1), "fit: 640x400 holds scale 1");
	Check(!UI_Scale_Fits(640, 400, 2), "fit: 640x400 refuses scale 2");
	Check(UI_Scale_Fits(1920, 1080, 2), "fit: 1080p holds scale 2");
	Check(!UI_Scale_Fits(1920, 1080, 3), "fit: 1080p refuses scale 3, sidebar too short");
	Check(!UI_Scale_Fits(800, 1200, 3), "fit: narrow frame refuses scale 3, view too narrow");
	Check(!UI_Scale_Fits(1920, 1080, 0), "fit: scale 0 never fits");
	Check_Scale(4, 640, 400, 1, "setting 4 at 640x400 reduces to 1");
	Check_Scale(3, 1920, 1080, 2, "setting 3 at 1080p reduces to 2");
	Check_Scale(4, 2560, 1440, 3, "setting 4 at 1440p reduces to 3");
	Check_Scale(2, 1024, 768, 1, "setting 2 at 1024x768 reduces to 1");
}


void Test_Surfaces(void)
{
	Check(UI_Sidebar_Surface_Height(1080, 2) == 540, "sidebar surface: 1080p at 2 is 540 tall");
	Check(UI_Sidebar_Surface_Height(1440, 2) == 720, "sidebar surface: 1440p at 2 is 720 tall");
	Check(UI_Sidebar_Surface_Height(2160, 3) == 720, "sidebar surface: 4K at 3 is 720 tall");
	Check(UI_Sidebar_Surface_Height(1080, 1) == 1080, "sidebar surface: scale 1 keeps the frame height");
	Check(UI_Sidebar_Surface_Height(1440, 3) == 480, "sidebar surface: 1440p at 3 is 480 tall");
	Check(UI_Tab_Surface_Width(1920 - 336, 2) == 792, "tab surface: 1080p at 2 is 792 wide");
	Check(UI_Tab_Surface_Width(2560 - 336, 2) == 1112, "tab surface: 1440p at 2 is 1112 wide");
	Check(UI_Tab_Surface_Width(3840 - 504, 3) == 1112, "tab surface: 4K at 3 is 1112 wide");
	Check(UI_Tab_Surface_Width(2560 - 504, 3) == 685, "tab surface: 1440p at 3 floors the remainder");
	Check(UI_Tab_Surface_Width(1920 - 168, 1) == 1752, "tab surface: scale 1 keeps the composite width");
}


void Test_Mapping(void)
{
	UIPoint left{0, 0};
	UIPoint right{3840 - 504, 0};

	Check_Box(UI_HUD_To_Frame(UIBox{31, 139, 27, 27}, left, 1), UIBox{31, 139, 27, 27}, "map: scale 1 leaves a box unchanged");
	Check_Box(UI_HUD_To_Frame(UIBox{31, 139, 27, 27}, left, 2), UIBox{62, 278, 54, 54}, "map: scale 2 doubles a left sidebar box");
	Check_Box(UI_HUD_To_Frame(UIBox{24, 174, 64, 51}, right, 3), UIBox{3336 + 72, 522, 192, 153}, "map: scale 3 places a right sidebar box");
	Check_Point(UI_HUD_To_Frame(UIPoint{8, 173}, left, 2), UIPoint{16, 346}, "map: a point scales like a box");

	Check_Point(UI_Frame_To_HUD(UIPoint{62, 278}, left, 2), UIPoint{31, 139}, "unmap: a box corner comes back");
	Check_Point(UI_Frame_To_HUD(UIPoint{63, 279}, left, 2), UIPoint{31, 139}, "unmap: every frame pixel of a block maps to it");
	Check_Point(UI_Frame_To_HUD(UIPoint{61, 277}, left, 2), UIPoint{30, 138}, "unmap: the pixel before a block is the block before");
	Check_Point(UI_Frame_To_HUD(UIPoint{3336 + 72, 522}, right, 3), UIPoint{24, 174}, "unmap: a right sidebar corner comes back");
	Check_Point(UI_Frame_To_HUD(UIPoint{3335, 5}, right, 3), UIPoint{-1, 1}, "unmap: just left of the right sidebar is negative, not zero");
	Check_Point(UI_Frame_To_HUD(UIPoint{3333, 0}, right, 3), UIPoint{-1, 0}, "unmap: three pixels left is still the column before");
	Check_Point(UI_Frame_To_HUD(UIPoint{3332, 0}, right, 3), UIPoint{-2, 0}, "unmap: four pixels left is two columns before");

	Check(UI_Floor_Div(-1, 2) == -1, "floor div: -1/2 is -1");
	Check(UI_Floor_Div(-2, 2) == -1, "floor div: -2/2 is -1");
	Check(UI_Floor_Div(-3, 2) == -2, "floor div: -3/2 is -2");
	Check(UI_Floor_Div(5, 1) == 5, "floor div: divisor 1 is identity");
	Check(UI_Floor_Div(7, 3) == 2, "floor div: positive values truncate");

	for (int scale = 1; scale <= UI_SCALE_MAX; scale++) {
		bool round_trip = true;
		for (int x = -5; x < 200 && round_trip; x++) {
			UIPoint frame = UI_HUD_To_Frame(UIPoint{x, x + 3}, right, scale);
			for (int dx = 0; dx < scale && round_trip; dx++) {
				UIPoint back = UI_Frame_To_HUD(UIPoint{frame.X + dx, frame.Y + dx}, right, scale);
				round_trip = back == UIPoint{x, x + 3};
			}
		}
		char what[64];
		std::snprintf(what, sizeof(what), "round trip: every frame pixel at scale %d finds its HUD pixel", scale);
		Check(round_trip, what);
	}
}

} // namespace


int main(void)
{
	Test_Auto_Rule();
	Test_Setting();
	Test_Fit();
	Test_Surfaces();
	Test_Mapping();

	std::printf("%d failure(s)\n", Failures);
	return(Failures == 0 ? 0 : 1);
}
