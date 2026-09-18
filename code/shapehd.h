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


// Classic artwork enlarged to the scale an HD pack draws at, so that a pack which replaces
// some of the interface but not all of it still lays out as one picture.

// Every pixel grown to a square of the given factor, or the shape itself when it cannot be.
ShapeSet const * Scaled_Shape(ShapeSet const * shape, int factor);

// Drops every enlarged copy. Call it wherever the shapes themselves are refetched.
void Scaled_Shape_Reset(void);

// How many bytes of enlarged copies are held.
unsigned Scaled_Shape_Bytes(void);
