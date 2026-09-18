/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once


struct FontSource
{
	void const * Data = nullptr;
	int Scale = 1;				// Pixels drawn per classic pixel.
	bool Owned = false;			// A loose file held for the life of the process.
	int Size = 0;
};

// A loose font in the searched folders, or the archived one; Data is NULL when neither has it.
FontSource Fetch_Font_Source(char const * name);

// How many bytes of loose font files are held.
unsigned Font_Override_Bytes(void);
