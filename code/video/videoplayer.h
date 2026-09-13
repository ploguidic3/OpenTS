/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// A decoder for a movie held in a container file rather than a VQA. It hands back
// picture and sound in file order, one packet at a time, and knows nothing of the
// screen, the mixer or the clock; those belong to the caller.

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>


enum VideoPacketType {
	VIDEO_PACKET_NONE,
	VIDEO_PACKET_FRAME,
	VIDEO_PACKET_AUDIO,
	VIDEO_PACKET_END,
	VIDEO_PACKET_ERROR,
};


struct VideoPacket
{
	VideoPacketType Type = VIDEO_PACKET_NONE;

	// Where the packet sits in the movie, from its start.
	double Seconds = 0.0;

	// A frame, top-down, one byte each of blue, green, red and alpha per pixel with no
	// padding between rows. Empty for a sound packet.
	std::vector<uint8_t> Bgra;
	int Width = 0;
	int Height = 0;

	// Sound, 16 bit signed, channels interleaved. Empty for a picture packet.
	std::vector<int16_t> Pcm;
	unsigned Frames = 0;
};


class VideoPlayer
{
	public:
		virtual ~VideoPlayer(void) = default;

		// Opens a container file. The path is what the file system knows the file by.
		// Returns false, with nothing held, when the file is missing, unreadable or
		// holds no picture the decoder can produce.
		virtual bool Open(char const * path) = 0;
		virtual void Close(void) = 0;

		virtual int Width(void) const = 0;
		virtual int Height(void) const = 0;
		virtual double Frame_Rate(void) const = 0;

		virtual bool Has_Audio(void) const = 0;
		virtual unsigned Audio_Rate(void) const = 0;
		virtual unsigned Audio_Channels(void) const = 0;

		// Produces the next packet of either kind, in the order the file carries them,
		// so a sound packet tends to lead the picture it belongs with. END is returned
		// once both are exhausted and then again on every later call; ERROR means the
		// file gave out partway and no more packets will come.
		virtual void Read_Next(VideoPacket & packet) = 0;
};
