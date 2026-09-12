/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "always.h"

#include "cctooltip.h"

#include "_map.h"
#include "_rect.h"
#include "_surface.h"
#include "_xmouse.h"
#include "dialog.h"
#include "dsurface.h"
#include "goptions.h"
#include "scheme.h"
#include "uilayout.h"
#include "wwfont.h"

#include "color.hh"

#include <algorithm>


/// <summary>
/// Prepares a tooltip for display.
/// This routine measures the text, word wraps it to fit whichever region it will appear
/// over -- the tactical map or the sidebar -- and then nudges the box so that it stays
/// within that region rather than hanging off the edge of it.
/// </summary>
/// <returns>bool; Is the tooltip fit to be shown? A hidden mouse suppresses it.</returns>
bool CCToolTip::Update(ToolTipText * text)
{
	if (Get_Mouse_State() >= 0) {
		FontClass *font = Font_From_TPF(Style);
		char *string = text->Text;
		Rect rect;
		font->String_Pixel_Bounds(text->Text, rect);
		rect.Width += 4;
		rect.Height += 3;
		text->TextWidth = std::max(rect.Width, text->TextWidth);
		text->TextHeight = std::max(rect.Height, text->TextHeight);

		Rect * trect;
		if (Options.IsSidebarOnRight == true) {
			if (text->Pos.x <= TacticalRect.X + TacticalRect.Width) {
				trect = &TacticalRect;
			} else {
				trect = &SidebarRect;
				Map.SidebarClass::IsToRedraw = true;
			}
		} else {
			if (text->Pos.x <= SidebarRect.X + SidebarRect.Width) {
				trect = &SidebarRect;
				Map.SidebarClass::IsToRedraw = true;
			} else {
				trect = &TacticalRect;
			}
		}

		if (trect != NULL) {

			// The box is measured in HUD pixels and placed in frame pixels.
			int scale = UI_Scale();

			if (text->TextWidth * scale >= trect->Width) {
				Format_Window_String(string, font, trect->Width / scale - 4, text->TextWidth, text->TextHeight);
				font->String_Pixel_Bounds(text->Text, rect);
				rect.Width += 4;
				rect.Height += 3;
				text->TextWidth = std::max(rect.Width, text->TextWidth);
				text->TextHeight = std::max(rect.Height, text->TextHeight);
			}

			int x = text->Pos.x + text->TextWidth * scale - trect->Width - trect->X;
			if (x > 0) {
				text->Pos.x -= x;
			}

			text->Pos.y += 16 * scale;
			if (text->Pos.y + text->TextHeight * scale - trect->Height - trect->Y > 0) {
				text->Pos.y = text->Pos.y - text->TextHeight * scale - 16 * scale;
			}
			if (text->Pos.y < trect->Y) {
				text->Pos.y = trect->Y;
			}
		}

		return(true);
	}

	return(false);
}


/// <summary>
/// Takes down the tooltip that is being displayed.
/// This routine flags whatever the tooltip was covering for a redraw, so that the map or
/// the sidebar paints back over the hole it leaves, and then lets the tooltip manager
/// forget it.
/// </summary>
void CCToolTip::Reset(const ToolTipText * text)
{
	bool redraw = false;
	if (Options.IsSidebarOnRight == true) {
		if (text->Pos.x >= TacticalRect.X + TacticalRect.Width) {
			redraw = true;
		}
	} else {
		if (text->Pos.x <= SidebarRect.X + SidebarRect.Width) {
			redraw = true;
		}
	}

	if (redraw) {
		Map.SidebarClass::IsToRedraw = true;
		Map.SidebarClass::IsForceCompleteRedraw = true;
	}
	Map.Flag_To_Redraw();
	BASECLASS::Reset(text);
}


/// <summary>
/// Draws the tooltip that is currently active.
/// This routine records which surface the draw is destined for before handing the work
/// over to the tooltip manager.
/// </summary>
/// <param name="sidebar">Is the sidebar surface the one being drawn to?</param>
void CCToolTip::Draw_Current(bool sidebar)
{
	UseSidebarSurface = sidebar;

	// Placement flags the sidebar for a redraw, and the sidebar clears that flag at the end
	// of its own draw, so placement has to be worked out in the tactical pass.
	BASECLASS::Draw_Current(!sidebar);
}


/// <summary>
/// Draws the tooltip box and its text.
/// This routine works out which of the game's surfaces the tooltip falls upon -- the
/// tactical composite or the sidebar -- and paints the frame and the text there. A
/// tooltip that lands across the boundary between the two is not drawn at all.
/// </summary>
void CCToolTip::Draw(const ToolTipText * text)
{
	Point2D point = Point2D(text->Pos.x, text->Pos.y);
	Surface * surface = NULL;
	int framewidth = text->TextWidth * UI_Scale();

	if (Options.IsSidebarOnRight == true) {
		int offset = TacticalRect.X + TacticalRect.Width;
		if (point.X + framewidth <= offset) {
			surface = CompositeSurface;
		} else if (UseSidebarSurface == true && point.X >= offset) {
			surface = SidebarSurface;
			point = Frame_To_Sidebar(point);
			Map.IsToBlitSidebar = true;
		}
	} else {
		int offset = SidebarRect.X + SidebarRect.Width;
		if (point.X >= offset) {
			surface = CompositeSurface;
			point.X -= offset;
		} else if (UseSidebarSurface == true && point.X + framewidth <= offset) {
			surface = SidebarSurface;
			point = Frame_To_Sidebar(point);
			Map.IsToBlitSidebar = true;
		}
	}

	if (surface != NULL) {
		auto paint = [text, this](Surface & target, Point2D const & at) {
			Rect drawrect(at.X, at.Y, text->TextWidth, text->TextHeight);
			target.Fill_Rect(drawrect, TBLACK);
			target.Draw_Rect(drawrect, DSurface::Build_Hicolor_Pixel(0, 255, 0));
			Fancy_Text_Print(text->Text, target, drawrect, Point2D(2, 1), Fetch_Scheme_By_Name("Green"), TBLACK, Style);
		};

		// The sidebar is magnified as a whole; the composite is not, so its box is drawn scaled.
		if (surface == SidebarSurface) {
			paint(*surface, point);
		} else {
			UI_Draw_Scaled(*surface, point, text->TextWidth, text->TextHeight, false, paint);
		}
	}
}


/// <summary>
/// Fetches the text to display for a tooltip.
/// This routine is called by the tooltip manager when a region becomes due for display.
/// The game map supplies the string, and nothing is offered while the mouse is hidden.
/// </summary>
/// <returns>Returns with a pointer to the help text, or NULL if there is none.</returns>
const char * CCToolTip::ToolTip_Text(int id)
{
	if (Get_Mouse_State() < 0) {
		return(NULL);
	}
	return(Map.Help_Text(id));
}
