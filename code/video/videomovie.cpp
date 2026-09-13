/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// Plays a container movie the way VQAClass::Play_VQA plays a VQA: the same message
// pump, the same skip hook every pass, the same sleep while the window is out of
// focus, and a picture that follows the sound track's clock.

#include "always.h"

#include "video/videomovie.h"

#include "_xmouse.h"
#include "ccfile.h"
#include "dbgprint.h"
#include "dsurface.h"
#include "globals.h"
#include "goptions.h"
#include "movieskip.h"
#include "session.h"
#include "theme.h"
#include "video.h"
#include "video/mfplayer.h"
#include "video/videoplayer.h"
#include "video/videosink.h"
#include "vqa.h"
#include "win.h"

#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <vector>

namespace {

// Sound decoded ahead of the picture before and during playback.
double const PRIME_SECONDS = 0.2;
double const LEAD_SECONDS = 0.5;

// Frames decoded but not yet due. A 4K frame is 33 MB, so the queue stays short.
size_t const MAX_PENDING_FRAMES = 3;

// How long the end waits for the last of the sound after the last frame.
unsigned long const DRAIN_TIMEOUT_MS = 2000;

// The overlay is drawn as if onto a movie this tall and grown to the picture with it.
int const OVERLAY_WIDTH = 512;
int const OVERLAY_HEIGHT = 96;
int const OVERLAY_REFERENCE_HEIGHT = 400;
int const OVERLAY_PADDING = 6;
unsigned int const OVERLAY_BOX_ALPHA = 0xBF000000;


struct OverlayImage
{
	std::vector<unsigned int> Bgra;
	int Width = 0;
	int Height = 0;
	int X = 0;
	int Y = 0;
};


unsigned int Widen_565(unsigned short pixel)
{
	unsigned int red = (unsigned int)(((pixel >> 11) & 0x1F) * 255 / 31);
	unsigned int green = (unsigned int)(((pixel >> 5) & 0x3F) * 255 / 63);
	unsigned int blue = (unsigned int)(pixel & 0x1F) * 255 / 31;
	return(0xFF000000 | (red << 16) | (green << 8) | blue);
}


// Draws the skip vote onto a small surface and lifts the part that was drawn on,
// pure black standing for the translucent box behind the text.
bool Build_Overlay(DSurface & surface, OverlayImage & image)
{
	image.Width = 0;
	image.Height = 0;
	surface.Fill(0);
	MovieSkip::Draw_Overlay(surface, Rect(0, 0, surface.Get_Width(), surface.Get_Height()));

	unsigned short const * pixels = (unsigned short const *)surface.Lock();
	if (pixels == NULL) {
		return(false);
	}
	int const stride = surface.Stride() / (int)sizeof(unsigned short);
	int const width = surface.Get_Width();
	int const height = surface.Get_Height();

	int left = width;
	int top = height;
	int right = -1;
	int bottom = -1;
	for (int y = 0; y < height; y++) {
		unsigned short const * row = pixels + y * stride;
		for (int x = 0; x < width; x++) {
			if (row[x] != 0) {
				if (x < left) left = x;
				if (x > right) right = x;
				if (y < top) top = y;
				if (y > bottom) bottom = y;
			}
		}
	}

	if (right < 0) {
		surface.Unlock();
		return(false);
	}

	left = left > OVERLAY_PADDING ? left - OVERLAY_PADDING : 0;
	top = top > OVERLAY_PADDING ? top - OVERLAY_PADDING : 0;
	right = right + OVERLAY_PADDING < width ? right + OVERLAY_PADDING : width - 1;
	bottom = bottom + OVERLAY_PADDING < height ? bottom + OVERLAY_PADDING : height - 1;

	image.X = left;
	image.Y = top;
	image.Width = right - left + 1;
	image.Height = bottom - top + 1;
	image.Bgra.resize((size_t)image.Width * (size_t)image.Height);
	for (int y = 0; y < image.Height; y++) {
		unsigned short const * row = pixels + (top + y) * stride + left;
		unsigned int * dest = &image.Bgra[(size_t)y * (size_t)image.Width];
		for (int x = 0; x < image.Width; x++) {
			dest[x] = row[x] == 0 ? OVERLAY_BOX_ALPHA : Widen_565(row[x]);
		}
	}
	surface.Unlock();
	return(true);
}


struct PendingFrame
{
	std::vector<uint8_t> Bgra;
	int Width = 0;
	int Height = 0;
	double Seconds = 0.0;
};


class PlaybackClass
{
	public:
		PlaybackClass(VideoPlayer & player, VideoSinkClass & sink) : Player(player), Sink(sink) {}

		// Decodes until a frame beyond the clock and enough sound are in hand.
		void Decode_Ahead(double clock);
		bool Present(double clock, bool integerfit, DSurface * overlaysurface);

		bool Exhausted = false;
		bool Presented = false;
		std::deque<PendingFrame> Frames;

	private:
		VideoPlayer & Player;
		VideoSinkClass & Sink;
		VideoPacket Packet;
		OverlayImage Overlay;
};


void PlaybackClass::Decode_Ahead(double clock)
{
	while (!Exhausted) {
		bool wantframe = Frames.empty() || (Frames.size() < MAX_PENDING_FRAMES && Frames.back().Seconds <= clock);
		bool wantaudio = Sink.Has_Audio() && Sink.Queued_Seconds() < LEAD_SECONDS && Frames.size() < MAX_PENDING_FRAMES;
		if (!wantframe && !wantaudio) {
			break;
		}

		Player.Read_Next(Packet);
		switch (Packet.Type) {
			case VIDEO_PACKET_FRAME: {
				PendingFrame frame;
				frame.Bgra.swap(Packet.Bgra);
				frame.Width = Packet.Width;
				frame.Height = Packet.Height;
				frame.Seconds = Packet.Seconds;
				Frames.push_back(std::move(frame));
				break;
			}

			case VIDEO_PACKET_AUDIO:
				Sink.Queue(Packet.Pcm.data(), Packet.Frames);
				break;

			case VIDEO_PACKET_ERROR:
				DebugString("Video: decoding stopped early\n");
				Exhausted = true;
				Sink.Mark_End();
				break;

			default:
				Exhausted = true;
				Sink.Mark_End();
				break;
		}
	}
}


bool PlaybackClass::Present(double clock, bool integerfit, DSurface * overlaysurface)
{
	// The newest frame that is due is shown; older ones the clock ran past are dropped.
	if (Frames.empty() || Frames.front().Seconds > clock) {
		return(false);
	}
	while (Frames.size() > 1 && Frames[1].Seconds <= clock) {
		Frames.pop_front();
	}
	PendingFrame & frame = Frames.front();

	bool hasoverlay = overlaysurface != NULL && Build_Overlay(*overlaysurface, Overlay);
	bool shown = Video_Present_Video_Frame(frame.Bgra.data(), frame.Width * 4, frame.Width, frame.Height, integerfit,
		hasoverlay ? Overlay.Bgra.data() : NULL, Overlay.Width * 4, Overlay.Width, Overlay.Height, Overlay.X, Overlay.Y, OVERLAY_REFERENCE_HEIGHT);
	Frames.pop_front();
	if (shown) {
		Presented = true;
	}
	return(shown);
}

} // namespace


bool Find_Video_File(char const * moviename, char * path, size_t size)
{
	if (moviename == NULL || *moviename == '\0' || path == NULL || size == 0) {
		return(false);
	}

	char stem[64];
	std::strncpy(stem, moviename, sizeof(stem) - 1);
	stem[sizeof(stem) - 1] = '\0';
	char * dot = std::strrchr(stem, '.');
	if (dot != NULL && dot != stem) {
		*dot = '\0';
	}

	char name[80];
	std::snprintf(name, sizeof(name), "%s.mp4", stem);

	CCFileClass file(name);
	if (!file.Is_Available()) {
		return(false);
	}
	std::strncpy(path, file.File_Name(), size - 1);
	path[size - 1] = '\0';
	return(true);
}


bool Play_Video_File(char const * path, ThemeType theme, bool stretch, bool nobreakout)
{
	if (path == NULL || !MF_Video_Available()) {
		return(false);
	}

	std::unique_ptr<VideoPlayer> player(Create_MF_Video_Player());
	if (!player || !player->Open(path)) {
		DebugString("Video: \"%s\" did not open; the VQA plays instead\n", path);
		return(false);
	}
	if (player->Width() < 320 && player->Height() < 200) {
		DebugString("Video: \"%s\" is too small to show\n", path);
		return(false);
	}
	DebugString("Video: playing \"%s\" %dx%d at %.2f fps%s\n", path, player->Width(), player->Height(), player->Frame_Rate(), player->Has_Audio() ? "" : " (no sound)");

	VideoSinkClass sink;
	if (player->Has_Audio()) {
		sink.Open(player->Audio_Rate(), player->Audio_Channels(), 1.0f);
	}

	// The other machines wait for this one, so the movie runs on without the window's focus.
	bool const pauseonfocusloss = !(Session.Type == GAME_IPX || Session.Type == GAME_INTERNET);
	bool const integerfit = !(stretch && Options.StretchMovies);

	std::unique_ptr<DSurface> overlaysurface(new DSurface(OVERLAY_WIDTH, OVERLAY_HEIGHT));
	PlaybackClass playback(*player, sink);

	Hide_Mouse();
	if (theme != THEME_NONE) {
		Theme.Queue_Song(theme);
	}
	Video_Begin_Movie();

	playback.Decode_Ahead(0.0);
	while (!playback.Exhausted && sink.Has_Audio() && sink.Queued_Seconds() < PRIME_SECONDS && playback.Frames.size() < MAX_PENDING_FRAMES) {
		playback.Decode_Ahead(playback.Frames.empty() ? 0.0 : playback.Frames.back().Seconds);
	}
	sink.Start();

	bool brokeout = false;
	bool sleeping = false;
	unsigned long drainstart = 0;

	while (true) {
		VQA_Message_Handler();

		if (MovieSkip::Idle() && !nobreakout) {
			brokeout = true;
			break;
		}

		if (!GameInFocus && pauseonfocusloss) {
			if (!sleeping) {
				sleeping = true;
				sink.Pause();
				DebugString("Movie is sleeping\n");
			}
			Sleep(1000 / 30);
			continue;
		}
		if (sleeping) {
			sleeping = false;
			sink.Resume();
			DebugString("Movie is awake\n");
		}

		double clock = sink.Clock_Seconds();
		playback.Decode_Ahead(clock);

		if (playback.Present(clock, integerfit, overlaysurface.get())) {
			continue;
		}

		// A frame the renderer refused before anything was shown lets the VQA play instead.
		if (!playback.Presented && playback.Frames.empty() && playback.Exhausted) {
			break;
		}

		if (playback.Exhausted && playback.Frames.empty()) {
			if (drainstart == 0) {
				drainstart = timeGetTime();
			}
			if (sink.Is_Drained() || (unsigned long)timeGetTime() - drainstart >= DRAIN_TIMEOUT_MS) {
				break;
			}
		}

		Sleep(1);
	}

	sink.Stop();
	sink.Release();
	player->Close();
	Video_End_Movie();
	Show_Mouse();

	if (brokeout) {
		return(true);
	}
	return(playback.Presented);
}
