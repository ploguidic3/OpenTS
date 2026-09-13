/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once

#include "theme.hh"

#include <cstddef>

// Looks for a container movie standing in for the named one: the name without its
// extension plus ".mp4", found the way any loose file is. Fills path with where it was
// found and returns true; false leaves path untouched.
bool Find_Video_File(char const * moviename, char * path, size_t size);

// Plays a container movie full screen, at the window's own size, and returns once it
// is over or skipped. False means nothing was shown, so the caller may play the VQA
// instead; a movie that fails partway through counts as shown.
bool Play_Video_File(char const * path, ThemeType theme, bool stretch, bool nobreakout);
