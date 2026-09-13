/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The sound track of a container movie, pushed into the mixer the way a VQA's is,
// and the clock its picture follows: what the mixer has heard of that track, or
// wall time for a movie with no track.

#pragma once

#include "audio/audiomovieclock.h"
#include "audio/audiostream.h"
#include "audio/audiohandle.h"

#include <cstdint>
#include <mutex>
#include <vector>


class VideoSinkClass : public AudioStreamProducerClass
{
	public:
		VideoSinkClass(void) = default;
		~VideoSinkClass(void) override;

		VideoSinkClass(VideoSinkClass const &) = delete;
		VideoSinkClass & operator=(VideoSinkClass const &) = delete;

		// Takes a mixer stream for a track of this shape. False leaves the sink silent,
		// and then the clock runs on wall time.
		bool Open(unsigned rate, unsigned channels, float level);
		void Release(void);
		bool Has_Audio(void) const { return(Stream != nullptr); }

		// Game thread. Queues decoded sound for the feeder to push; the queue is not
		// bounded, so the caller stops decoding once Queued_Seconds is enough. seconds is
		// where the first frame of this block sits in the movie.
		void Queue(int16_t const * pcm, unsigned frames, double seconds);
		double Queued_Seconds(void);
		void Mark_End(void);

		// Starts the clock, and the voice where there is one. Pause holds both.
		void Start(void);
		void Pause(void);
		void Resume(void);
		void Stop(void);

		// Seconds of the movie that have been heard, or elapsed for a silent movie and
		// until the first of the sound has been heard.
		double Clock_Seconds(void);

		// Whether every queued sample has left the mixer, or the voice is gone.
		bool Is_Drained(void);

		// The feeder's own hooks. Close does nothing: the stream is released by Release,
		// on the game thread, once the feeder has let go.
		bool Fill(AudioStreamClass & stream) override;
		void Close(void) override {}
		unsigned Min_Ring_Frames(void) const override { return(1); }

	private:
		std::mutex Lock;
		std::vector<int16_t> Queued;
		size_t QueuedHead = 0;
		bool Ended = false;

		int Slot = -1;
		AudioStreamClass * Stream = nullptr;
		AudioHandle Handle;
		AudioPushStreamProducerClass Pusher;
		AudioMovieClockClass Clock;
		unsigned Rate = 0;
		unsigned Channels = 0;
		unsigned Latency = 0;
		double Origin = 0.0;
		bool HasOrigin = false;
		float Level = 1.0f;
		bool Started = false;
		bool Paused = false;
		bool Attached = false;

		unsigned long WallStart = 0;
		unsigned long PausedAt = 0;
		unsigned long PausedTotal = 0;
};
