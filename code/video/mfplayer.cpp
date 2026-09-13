/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// The Media Foundation side of the movie player. This is the only translation unit that
// includes Media Foundation, and it reaches nothing of the engine but the debug log, so
// a harness can compile it on its own.

#include "video/mfplayer.h"

#include "video/videoplayer.h"

#include "dbgprint.h"

#include <windows.h>

#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#include <cstring>
#include <string>

namespace {

double const SECONDS_PER_HNS = 1.0 / 10000000.0;


template<class T>
class ComPtr
{
	public:
		ComPtr(void) = default;
		~ComPtr(void) { Release(); }
		ComPtr(ComPtr const &) = delete;
		ComPtr & operator=(ComPtr const &) = delete;

		T ** operator&(void) { Release(); return(&Pointer); }
		T * operator->(void) const { return(Pointer); }
		T * Get(void) const { return(Pointer); }
		explicit operator bool(void) const { return(Pointer != nullptr); }

		void Release(void)
		{
			if (Pointer != nullptr) {
				Pointer->Release();
				Pointer = nullptr;
			}
		}

	private:
		T * Pointer = nullptr;
};


std::wstring To_Wide(char const * path)
{
	std::wstring wide;
	int length = MultiByteToWideChar(CP_ACP, 0, path, -1, nullptr, 0);
	if (length > 0) {
		wide.resize((size_t)length - 1);
		MultiByteToWideChar(CP_ACP, 0, path, -1, &wide[0], length);
	}
	return(wide);
}


class MFVideoPlayer : public VideoPlayer
{
	public:
		~MFVideoPlayer(void) override { Close(); }

		bool Open(char const * path) override;
		void Close(void) override;

		int Width(void) const override { return(FrameWidth); }
		int Height(void) const override { return(FrameHeight); }
		double Frame_Rate(void) const override { return(FrameRate); }

		bool Has_Audio(void) const override { return(AudioPresent); }
		unsigned Audio_Rate(void) const override { return(AudioRate); }
		unsigned Audio_Channels(void) const override { return(AudioChannels); }

		void Read_Next(VideoPacket & packet) override;

	private:
		void Find_Streams(void);
		bool Configure_Video(void);
		bool Configure_Audio(void);
		bool Read_Video_Type(void);
		bool Copy_Frame(IMFSample * sample, VideoPacket & packet);
		bool Copy_Audio(IMFSample * sample, VideoPacket & packet);

		ComPtr<IMFSourceReader> Reader;
		bool ComStarted = false;
		bool Started = false;
		bool VideoDone = true;
		bool AudioDone = true;
		bool Failed = false;

		int FrameWidth = 0;
		int FrameHeight = 0;
		double FrameRate = 0.0;
		LONG Stride = 0;
		int CropX = 0;
		int CropY = 0;
		int CropWidth = 0;
		int CropHeight = 0;

		DWORD VideoIndex = 0;
		DWORD AudioIndex = 0;

		bool AudioPresent = false;
		unsigned AudioRate = 0;
		unsigned AudioChannels = 0;
		unsigned SourceChannels = 0;
};


bool MFVideoPlayer::Open(char const * path)
{
	Close();
	if (path == nullptr || !MF_Video_Available()) {
		return(false);
	}

	// The decoders are COM objects; the game does not otherwise start COM on this thread.
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	ComStarted = SUCCEEDED(hr);

	hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
	if (FAILED(hr)) {
		DebugString("Video: MFStartup failed %08lx\n", (unsigned long)hr);
		return(false);
	}
	Started = true;

	ComPtr<IMFAttributes> attributes;
	if (FAILED(MFCreateAttributes(&attributes, 1))
		|| FAILED(attributes->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE))) {
		Close();
		return(false);
	}

	std::wstring wide = To_Wide(path);
	hr = MFCreateSourceReaderFromURL(wide.c_str(), attributes.Get(), &Reader);
	if (FAILED(hr)) {
		DebugString("Video: cannot open \"%s\" (%08lx)\n", path, (unsigned long)hr);
		Close();
		return(false);
	}

	Reader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
	Find_Streams();

	if (!Configure_Video()) {
		DebugString("Video: \"%s\" has no picture the decoder can produce\n", path);
		Close();
		return(false);
	}
	VideoDone = false;

	AudioPresent = Configure_Audio();
	AudioDone = !AudioPresent;
	return(true);
}


// The reader names streams by their own index when it hands a sample back, so the
// first picture and sound streams are located once rather than asked for by role.
void MFVideoPlayer::Find_Streams(void)
{
	VideoIndex = (DWORD)-1;
	AudioIndex = (DWORD)-1;
	for (DWORD index = 0; index < 64; index++) {
		ComPtr<IMFMediaType> type;
		HRESULT hr = Reader->GetNativeMediaType(index, 0, &type);
		if (hr == MF_E_INVALIDSTREAMNUMBER) {
			break;
		}
		if (FAILED(hr)) {
			continue;
		}
		GUID major = GUID_NULL;
		if (FAILED(type->GetGUID(MF_MT_MAJOR_TYPE, &major))) {
			continue;
		}
		if (major == MFMediaType_Video && VideoIndex == (DWORD)-1) {
			VideoIndex = index;
		} else if (major == MFMediaType_Audio && AudioIndex == (DWORD)-1) {
			AudioIndex = index;
		}
	}
}


bool MFVideoPlayer::Configure_Video(void)
{
	DWORD const stream = VideoIndex;
	if (stream == (DWORD)-1 || FAILED(Reader->SetStreamSelection(stream, TRUE))) {
		return(false);
	}

	ComPtr<IMFMediaType> type;
	if (FAILED(MFCreateMediaType(&type))
		|| FAILED(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video))
		|| FAILED(type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32))
		|| FAILED(Reader->SetCurrentMediaType(stream, nullptr, type.Get()))) {
		return(false);
	}
	return(Read_Video_Type());
}


bool MFVideoPlayer::Read_Video_Type(void)
{
	ComPtr<IMFMediaType> type;
	if (FAILED(Reader->GetCurrentMediaType(VideoIndex, &type))) {
		return(false);
	}

	GUID subtype = GUID_NULL;
	UINT32 width = 0;
	UINT32 height = 0;
	if (FAILED(type->GetGUID(MF_MT_SUBTYPE, &subtype)) || subtype != MFVideoFormat_RGB32
		|| FAILED(MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &width, &height))
		|| width == 0 || height == 0) {
		return(false);
	}

	UINT32 numerator = 0;
	UINT32 denominator = 0;
	if (SUCCEEDED(MFGetAttributeRatio(type.Get(), MF_MT_FRAME_RATE, &numerator, &denominator)) && denominator != 0) {
		FrameRate = (double)numerator / (double)denominator;
	}

	// The decoder may pad the picture to its block size; the aperture is what to show.
	CropX = 0;
	CropY = 0;
	CropWidth = (int)width;
	CropHeight = (int)height;
	MFVideoArea area;
	if (SUCCEEDED(type->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, (UINT8 *)&area, sizeof(area), nullptr))) {
		int x = area.OffsetX.value;
		int y = area.OffsetY.value;
		int w = area.Area.cx;
		int h = area.Area.cy;
		if (x >= 0 && y >= 0 && w > 0 && h > 0 && x + w <= (int)width && y + h <= (int)height) {
			CropX = x;
			CropY = y;
			CropWidth = w;
			CropHeight = h;
		}
	}

	UINT32 stride = 0;
	if (SUCCEEDED(type->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride))) {
		Stride = (LONG)(INT32)stride;
	} else {
		Stride = (LONG)width * 4;
	}

	FrameWidth = CropWidth;
	FrameHeight = CropHeight;
	return(true);
}


bool MFVideoPlayer::Configure_Audio(void)
{
	DWORD const stream = AudioIndex;
	if (stream == (DWORD)-1 || FAILED(Reader->SetStreamSelection(stream, TRUE))) {
		return(false);
	}

	ComPtr<IMFMediaType> type;
	if (FAILED(MFCreateMediaType(&type))
		|| FAILED(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio))
		|| FAILED(type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM))
		|| FAILED(type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16))
		|| FAILED(Reader->SetCurrentMediaType(stream, nullptr, type.Get()))) {
		Reader->SetStreamSelection(stream, FALSE);
		return(false);
	}

	ComPtr<IMFMediaType> current;
	UINT32 rate = 0;
	UINT32 channels = 0;
	UINT32 bits = 0;
	if (FAILED(Reader->GetCurrentMediaType(stream, &current))
		|| FAILED(current->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate))
		|| FAILED(current->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels))
		|| FAILED(current->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits))
		|| rate == 0 || channels == 0 || bits != 16) {
		Reader->SetStreamSelection(stream, FALSE);
		return(false);
	}

	AudioRate = rate;
	SourceChannels = channels;
	AudioChannels = channels > 2 ? 2 : channels;
	return(true);
}


void MFVideoPlayer::Close(void)
{
	Reader.Release();
	if (Started) {
		MFShutdown();
		Started = false;
	}
	if (ComStarted) {
		CoUninitialize();
		ComStarted = false;
	}
	VideoDone = true;
	AudioDone = true;
	Failed = false;
	AudioPresent = false;
	FrameWidth = 0;
	FrameHeight = 0;
	FrameRate = 0.0;
	AudioRate = 0;
	AudioChannels = 0;
	SourceChannels = 0;
}


bool MFVideoPlayer::Copy_Frame(IMFSample * sample, VideoPacket & packet)
{
	ComPtr<IMFMediaBuffer> buffer;
	if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) {
		return(false);
	}

	int const width = CropWidth;
	int const height = CropHeight;
	packet.Bgra.resize((size_t)width * (size_t)height * 4);

	// A 2D buffer knows its own layout; a plain one is read with the type's stride,
	// which is negative when the rows are stored bottom up.
	ComPtr<IMF2DBuffer> buffer2d;
	BYTE * scanline0 = nullptr;
	LONG pitch = 0;
	BYTE * plain = nullptr;
	bool locked2d = false;

	if (SUCCEEDED(buffer->QueryInterface(IID_PPV_ARGS(&buffer2d))) && SUCCEEDED(buffer2d->Lock2D(&scanline0, &pitch))) {
		locked2d = true;
	} else {
		buffer2d.Release();
		DWORD length = 0;
		if (FAILED(buffer->Lock(&plain, nullptr, &length))) {
			return(false);
		}
		LONG const rowbytes = Stride < 0 ? -Stride : Stride;
		if (rowbytes < (LONG)FrameWidth * 4 || (DWORD)rowbytes * (DWORD)(CropY + CropHeight) > length) {
			buffer->Unlock();
			return(false);
		}
		if (Stride < 0) {
			DWORD fullrows = length / (DWORD)rowbytes;
			scanline0 = plain + (size_t)rowbytes * (size_t)(fullrows - 1);
			pitch = -rowbytes;
		} else {
			scanline0 = plain;
			pitch = rowbytes;
		}
	}

	for (int y = 0; y < height; y++) {
		BYTE const * source = scanline0 + (ptrdiff_t)pitch * (CropY + y) + (ptrdiff_t)CropX * 4;
		std::memcpy(&packet.Bgra[(size_t)y * (size_t)width * 4], source, (size_t)width * 4);
	}

	if (locked2d) {
		buffer2d->Unlock2D();
	} else {
		buffer->Unlock();
	}

	packet.Width = width;
	packet.Height = height;
	return(true);
}


bool MFVideoPlayer::Copy_Audio(IMFSample * sample, VideoPacket & packet)
{
	ComPtr<IMFMediaBuffer> buffer;
	if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) {
		return(false);
	}
	BYTE * data = nullptr;
	DWORD length = 0;
	if (FAILED(buffer->Lock(&data, nullptr, &length))) {
		return(false);
	}

	int16_t const * samples = (int16_t const *)data;
	unsigned const frames = length / (SourceChannels * sizeof(int16_t));
	packet.Pcm.resize((size_t)frames * AudioChannels);
	if (SourceChannels == AudioChannels) {
		std::memcpy(packet.Pcm.data(), samples, (size_t)frames * AudioChannels * sizeof(int16_t));
	} else {
		// Only the front pair of a wider layout is kept.
		for (unsigned i = 0; i < frames; i++) {
			packet.Pcm[(size_t)i * 2] = samples[(size_t)i * SourceChannels];
			packet.Pcm[(size_t)i * 2 + 1] = samples[(size_t)i * SourceChannels + 1];
		}
	}
	buffer->Unlock();
	packet.Frames = frames;
	return(true);
}


void MFVideoPlayer::Read_Next(VideoPacket & packet)
{
	packet.Type = VIDEO_PACKET_NONE;
	packet.Seconds = 0.0;
	packet.Bgra.clear();
	packet.Pcm.clear();
	packet.Frames = 0;
	packet.Width = 0;
	packet.Height = 0;

	if (Failed) {
		packet.Type = VIDEO_PACKET_ERROR;
		return;
	}
	if (!Reader || (VideoDone && AudioDone)) {
		packet.Type = VIDEO_PACKET_END;
		return;
	}

	// A stream that ended keeps answering with the end flag until the other does, so the
	// read is repeated over such answers rather than reported each time.
	for (int attempt = 0; attempt < 8; attempt++) {
		DWORD stream = 0;
		DWORD flags = 0;
		LONGLONG timestamp = 0;
		ComPtr<IMFSample> sample;
		HRESULT hr = Reader->ReadSample((DWORD)MF_SOURCE_READER_ANY_STREAM, 0, &stream, &flags, &timestamp, &sample);
		if (FAILED(hr) || (flags & MF_SOURCE_READERF_ERROR) != 0) {
			DebugString("Video: read failed %08lx\n", (unsigned long)hr);
			Failed = true;
			packet.Type = VIDEO_PACKET_ERROR;
			return;
		}

		bool const isvideo = stream == VideoIndex;
		bool const isaudio = AudioPresent && stream == AudioIndex;
		if (!isvideo && !isaudio) {
			continue;
		}
		if ((flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) != 0 && isvideo) {
			if (!Read_Video_Type()) {
				Failed = true;
				packet.Type = VIDEO_PACKET_ERROR;
				return;
			}
		}

		if ((flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0) {
			if (isvideo) {
				VideoDone = true;
			} else {
				AudioDone = true;
			}
		}

		if (sample) {
			bool copied = isvideo ? Copy_Frame(sample.Get(), packet) : Copy_Audio(sample.Get(), packet);
			if (!copied) {
				Failed = true;
				packet.Type = VIDEO_PACKET_ERROR;
				return;
			}
			packet.Type = isvideo ? VIDEO_PACKET_FRAME : VIDEO_PACKET_AUDIO;
			packet.Seconds = (double)timestamp * SECONDS_PER_HNS;
			return;
		}

		if (VideoDone && AudioDone) {
			packet.Type = VIDEO_PACKET_END;
			return;
		}
	}

	// Nothing arrived over several reads; treat the file as over rather than spin.
	packet.Type = VIDEO_PACKET_END;
}

} // namespace


bool MF_Video_Available(void)
{
	static int _available = -1;
	if (_available < 0) {
		HMODULE plat = LoadLibraryW(L"mfplat.dll");
		HMODULE readwrite = LoadLibraryW(L"mfreadwrite.dll");
		_available = (plat != nullptr && readwrite != nullptr) ? 1 : 0;
		if (readwrite != nullptr) {
			FreeLibrary(readwrite);
		}
		if (plat != nullptr) {
			FreeLibrary(plat);
		}
	}
	return(_available == 1);
}


VideoPlayer * Create_MF_Video_Player(void)
{
	if (!MF_Video_Available()) {
		return(nullptr);
	}
	return(new MFVideoPlayer);
}
