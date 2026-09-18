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

// The most bytes Scale_Encode_RLE_Frame can produce for a frame of this size.
int Scale_Encoded_RLE_Bound(int width, int height);

// Encodes width * height bytes of source into the row format the shape files carry and the
// RLE blitter walks, returning the bytes written. Dest must hold Scale_Encoded_RLE_Bound
// bytes. Returns zero if the frame cannot be encoded.
int Scale_Encode_RLE_Frame(unsigned char const * source, int width, int height, unsigned char * dest);

// Decodes one RLE frame into width * height bytes of dest. On malformed data it returns
// false having filled dest with the transparent index, so an unchecked caller draws nothing.
// A data_size of zero means the size is not known and each row's own length prefix is
// trusted to walk the rows, which is what the engine's RLE blitter does.
bool Scale_Decode_RLE_Frame(void const * data, int data_size, int width, int height, unsigned char * dest);
