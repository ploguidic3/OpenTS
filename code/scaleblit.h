/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once


// Magnifying 8-bit shape and tile pixels for the scaled tactical view.

// Magnifies source into dest, which must hold width * scale * height * scale bytes; every
// output byte is one of the input bytes, so no palette index is blended into another.
void Scale_Expand_8Bit(unsigned char const * source, int width, int height, unsigned char * dest, int scale);

// Decodes one RLE frame into width * height bytes of dest. On malformed data it returns
// false having filled dest with the transparent index, so an unchecked caller draws nothing.
bool Scale_Decode_RLE_Frame(void const * data, int data_size, int width, int height, unsigned char * dest);
