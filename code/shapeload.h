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

struct ShapeSource
{
	ShapeSet const * Shape = nullptr;
	int Scale = 1;				// Pixels drawn per classic pixel.
	bool Owned = false;			// A loose file held for the life of the process.
	int Size = 0;
};

// A loose file in the searched folders wins over the cached archives; NULL when neither has it.
ShapeSet const * Fetch_Shape(char const * name);

// The same lookup with where it landed; Shape is NULL when nothing was found.
ShapeSource Fetch_Shape_Source(char const * name);

// The scale a fetched shape was recognised at; 1 for an archived or unknown shape.
int Shape_Scale(ShapeSet const * shape);

// Whether loose files are looked for at all. On until the AssetOverrides setting says otherwise.
void Enable_Shape_Overrides(bool enabled);
bool Shape_Overrides_Enabled(void);

// How many bytes of loose shape files are held.
unsigned Shape_Override_Bytes(void);
