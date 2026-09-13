/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "always.h"

#include "video/videosink.h"

#include "audio/audiodefs.hh"
#include "audio/audioengine.h"
#include "dbgprint.h"
#include "win.h"

#include <algorithm>
#include <cstring>

namespace {

// One block is a tenth of a second, so the ring holds what the VQA ring holds in blocks.
unsigned const BLOCK_SECONDS_DIVISOR = 10;

// How long Stop waits for the mixer to let go of the ring.
int const STOP_WAIT_MS = 250;

unsigned long Wall_Now(void)
{
	return((unsigned long)timeGetTime());
}

} // namespace


VideoSinkClass::~VideoSinkClass(void)
{
	Release();
}


bool VideoSinkClass::Open(unsigned rate, unsigned channels, float level)
{
	Release();
	if (!AudioEngine.Is_Available() || rate == 0 || channels == 0 || channels > 2) {
		return(false);
	}
	unsigned blockframes = rate / BLOCK_SECONDS_DIVISOR;
	if (blockframes == 0) {
		return(false);
	}

	AudioStreamClass * stream = nullptr;
	int slot = AudioEngine.Acquire_Stream_Slot(&stream);
	if (slot < 0) {
		DebugString("Video: no free stream for the movie\n");
		return(false);
	}
	if (!stream->Init(blockframes * AUDIO_MOVIE_RING_BLOCKS, channels, rate)) {
		AudioEngine.Release_Stream_Slot(slot);
		return(false);
	}

	Slot = slot;
	Stream = stream;
	Rate = rate;
	Channels = channels;
	Level = level;
	Pusher.Open(*stream);
	Latency = (unsigned)((uint64_t)AudioEngine.Device_Latency_Frames() * rate / AUDIO_MIX_RATE);
	Clock.Reset(rate, blockframes, Latency, 1000);
	Origin = 0.0;
	HasOrigin = false;
	return(true);
}


void VideoSinkClass::Release(void)
{
	Stop();
	if (Stream != nullptr) {
		Pusher.Close();
		AudioEngine.Release_Stream_Slot(Slot);
		Slot = -1;
		Stream = nullptr;
	}
	std::lock_guard<std::mutex> guard(Lock);
	Queued.clear();
	QueuedHead = 0;
	Ended = false;
}


void VideoSinkClass::Queue(int16_t const * pcm, unsigned frames, double seconds)
{
	if (Stream == nullptr || pcm == nullptr || frames == 0) {
		return;
	}
	if (!HasOrigin) {
		Origin = seconds > 0.0 ? seconds : 0.0;
		HasOrigin = true;
	}
	std::lock_guard<std::mutex> guard(Lock);
	if (QueuedHead > 0 && QueuedHead * 2 > Queued.size()) {
		Queued.erase(Queued.begin(), Queued.begin() + (ptrdiff_t)QueuedHead);
		QueuedHead = 0;
	}
	Queued.insert(Queued.end(), pcm, pcm + (size_t)frames * Channels);
}


double VideoSinkClass::Queued_Seconds(void)
{
	if (Stream == nullptr) {
		return(0.0);
	}
	std::lock_guard<std::mutex> guard(Lock);
	size_t frames = (Queued.size() - QueuedHead) / Channels;
	frames += Stream->Ring.Available_Read();
	return((double)frames / (double)Rate);
}


void VideoSinkClass::Mark_End(void)
{
	std::lock_guard<std::mutex> guard(Lock);
	Ended = true;
}


void VideoSinkClass::Start(void)
{
	if (Started) {
		return;
	}
	Started = true;
	Paused = false;
	WallStart = Wall_Now();
	PausedTotal = 0;

	if (Stream == nullptr) {
		return;
	}
	Stream->Reset();
	Fill(*Stream);
	if (!AudioEngine.Feeder_Ref().Attach((unsigned)Slot, Stream, this)) {
		DebugString("Video: the feeder refused the movie stream\n");
		return;
	}
	Attached = true;
	Handle = AudioEngine.Events().Start_Stream(Stream, AUDIO_GROUP_MOVIE, Level, 0.0f);
	if (Handle.Is_Null()) {
		AudioEngine.Feeder_Ref().Detach((unsigned)Slot);
		Attached = false;
	}
}


void VideoSinkClass::Pause(void)
{
	if (!Started || Paused) {
		return;
	}
	Paused = true;
	PausedAt = Wall_Now();
	if (!Handle.Is_Null()) {
		AudioEngine.Events().Pause(Handle);
	}
	Clock.Pause();
}


void VideoSinkClass::Resume(void)
{
	if (!Started || !Paused) {
		return;
	}
	Paused = false;
	PausedTotal += Wall_Now() - PausedAt;
	if (!Handle.Is_Null()) {
		AudioEngine.Events().Resume(Handle);
	}
	Clock.Resume(Wall_Now());
}


void VideoSinkClass::Stop(void)
{
	if (!Started) {
		return;
	}
	if (Attached) {
		AudioEngine.Feeder_Ref().Detach((unsigned)Slot);
		Attached = false;
	}
	if (!Handle.Is_Null()) {
		AudioEngine.Events().Stop(Handle);
		if (!AudioEngine.Wait_Finished(Handle, STOP_WAIT_MS)) {
			DebugString("Video: movie voice did not stop in time\n");
		}
		Handle.Clear();
	}
	if (Stream != nullptr) {
		Stream->Reset();
	}
	Started = false;
	Paused = false;
}


double VideoSinkClass::Clock_Seconds(void)
{
	if (!Started) {
		return(0.0);
	}
	unsigned long now = Paused ? PausedAt : Wall_Now();
	double wall = (double)(now - WallStart - PausedTotal) / 1000.0;

	// Until the first of the track has been heard the sound clock stands at zero, so a
	// movie whose sound starts late, or one whose device is slow to start, runs on
	// wall time until then.
	if (Handle.Is_Null() || Stream->Frames_Consumed() <= Latency) {
		return(wall);
	}
	return(Origin + (double)Clock.Ticks(Stream->Frames_Consumed(), 0, Wall_Now()) / 1000.0);
}


bool VideoSinkClass::Is_Drained(void)
{
	if (Handle.Is_Null()) {
		return(true);
	}
	std::lock_guard<std::mutex> guard(Lock);
	if (Queued.size() > QueuedHead) {
		return(false);
	}
	return(Stream->Ring.Available_Read() == 0 || AudioEngine.Events().Is_Finished(Handle));
}


// Feeder thread.
bool VideoSinkClass::Fill(AudioStreamClass & stream)
{
	std::lock_guard<std::mutex> guard(Lock);
	size_t available = (Queued.size() - QueuedHead) / Channels;
	unsigned room = Pusher.Free_Frames();
	unsigned count = (unsigned)std::min<size_t>(available, room);
	if (count > 0) {
		unsigned written = Pusher.Push(&Queued[QueuedHead], count * Channels * (unsigned)sizeof(int16_t), false);
		QueuedHead += (size_t)written * Channels;
	}
	if (Ended && Queued.size() == QueuedHead) {
		Pusher.Mark_End();
		return(false);
	}
	(void)stream;
	return(true);
}
