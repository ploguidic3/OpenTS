/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#pragma once

class VideoPlayer;

// Whether Windows Media Foundation is installed. False on the N editions and on a
// server without the media feature, where the decoder cannot be created at all.
bool MF_Video_Available(void);

// A player over the Media Foundation source reader. The caller owns the result;
// null when Media Foundation is unavailable.
VideoPlayer * Create_MF_Video_Player(void);
