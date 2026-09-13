/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// Exercises the Media Foundation player without the engine or any game data: what it
// reports for a movie the harness encodes itself, and how it refuses a file that is not
// a movie. Where Media Foundation or its H.264 encoder is missing the harness says so
// and passes, since that is the runner's state rather than the player's.

#include <windows.h>

#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "video/mfplayer.h"
#include "video/videoplayer.h"

namespace {

int Failures = 0;

int const WIDTH = 64;
int const HEIGHT = 48;
int const FRAMES = 12;
int const FPS = 15;
unsigned const AUDIO_RATE = 48000;
unsigned const AUDIO_CHANNELS = 2;
LONGLONG const HNS_PER_SECOND = 10000000;


void Check(bool condition, char const * what)
{
	std::printf("%-62s %s\n", what, condition ? "ok" : "FAILED");
	if (!condition) {
		Failures++;
	}
}


template<class T>
void Release(T *& pointer)
{
	if (pointer != nullptr) {
		pointer->Release();
		pointer = nullptr;
	}
}


std::wstring Temp_Path(wchar_t const * name)
{
	wchar_t folder[MAX_PATH];
	DWORD length = GetTempPathW(MAX_PATH, folder);
	std::wstring path(folder, length);
	path += L"opents-videoplayer-";
	path += std::to_wstring((unsigned long)GetCurrentProcessId());
	path += L"-";
	path += name;
	return(path);
}


std::string Narrow(std::wstring const & wide)
{
	int length = WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
	std::string narrow;
	if (length > 0) {
		narrow.resize((size_t)length - 1);
		WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, &narrow[0], length, nullptr, nullptr);
	}
	return(narrow);
}


bool Write_Sample(IMFSinkWriter * writer, DWORD stream, void const * data, DWORD bytes, LONGLONG time, LONGLONG duration)
{
	IMFMediaBuffer * buffer = nullptr;
	IMFSample * sample = nullptr;
	BYTE * dest = nullptr;
	bool ok = SUCCEEDED(MFCreateMemoryBuffer(bytes, &buffer))
		&& SUCCEEDED(buffer->Lock(&dest, nullptr, nullptr));
	if (ok) {
		std::memcpy(dest, data, bytes);
		buffer->Unlock();
		ok = SUCCEEDED(buffer->SetCurrentLength(bytes))
			&& SUCCEEDED(MFCreateSample(&sample))
			&& SUCCEEDED(sample->AddBuffer(buffer))
			&& SUCCEEDED(sample->SetSampleTime(time))
			&& SUCCEEDED(sample->SetSampleDuration(duration))
			&& SUCCEEDED(writer->WriteSample(stream, sample));
	}
	Release(sample);
	Release(buffer);
	return(ok);
}


// Encodes a short H.264 and AAC movie. False when the runner lacks the encoders.
bool Encode_Movie(std::wstring const & path, std::string & why)
{
	IMFSinkWriter * writer = nullptr;
	IMFMediaType * type = nullptr;
	DWORD videostream = 0;
	DWORD audiostream = 0;
	bool ok = true;

	if (FAILED(MFCreateSinkWriterFromURL(path.c_str(), nullptr, nullptr, &writer))) {
		why = "the sink writer could not be created";
		return(false);
	}

	ok = SUCCEEDED(MFCreateMediaType(&type))
		&& SUCCEEDED(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video))
		&& SUCCEEDED(type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264))
		&& SUCCEEDED(type->SetUINT32(MF_MT_AVG_BITRATE, 400000))
		&& SUCCEEDED(type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive))
		&& SUCCEEDED(MFSetAttributeSize(type, MF_MT_FRAME_SIZE, WIDTH, HEIGHT))
		&& SUCCEEDED(MFSetAttributeRatio(type, MF_MT_FRAME_RATE, FPS, 1))
		&& SUCCEEDED(MFSetAttributeRatio(type, MF_MT_PIXEL_ASPECT_RATIO, 1, 1))
		&& SUCCEEDED(writer->AddStream(type, &videostream));
	Release(type);
	if (!ok) {
		why = "the H.264 encoder is not available";
		Release(writer);
		return(false);
	}

	ok = SUCCEEDED(MFCreateMediaType(&type))
		&& SUCCEEDED(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video))
		&& SUCCEEDED(type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32))
		&& SUCCEEDED(type->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive))
		&& SUCCEEDED(type->SetUINT32(MF_MT_DEFAULT_STRIDE, WIDTH * 4))
		&& SUCCEEDED(MFSetAttributeSize(type, MF_MT_FRAME_SIZE, WIDTH, HEIGHT))
		&& SUCCEEDED(MFSetAttributeRatio(type, MF_MT_FRAME_RATE, FPS, 1))
		&& SUCCEEDED(MFSetAttributeRatio(type, MF_MT_PIXEL_ASPECT_RATIO, 1, 1))
		&& SUCCEEDED(writer->SetInputMediaType(videostream, type, nullptr));
	Release(type);
	if (!ok) {
		why = "the encoder refused RGB32 input";
		Release(writer);
		return(false);
	}

	ok = SUCCEEDED(MFCreateMediaType(&type))
		&& SUCCEEDED(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio))
		&& SUCCEEDED(type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC))
		&& SUCCEEDED(type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16))
		&& SUCCEEDED(type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, AUDIO_RATE))
		&& SUCCEEDED(type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, AUDIO_CHANNELS))
		&& SUCCEEDED(type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 12000))
		&& SUCCEEDED(writer->AddStream(type, &audiostream));
	Release(type);
	if (!ok) {
		why = "the AAC encoder is not available";
		Release(writer);
		return(false);
	}

	ok = SUCCEEDED(MFCreateMediaType(&type))
		&& SUCCEEDED(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio))
		&& SUCCEEDED(type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM))
		&& SUCCEEDED(type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16))
		&& SUCCEEDED(type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, AUDIO_RATE))
		&& SUCCEEDED(type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, AUDIO_CHANNELS))
		&& SUCCEEDED(type->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, AUDIO_CHANNELS * 2))
		&& SUCCEEDED(type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, AUDIO_RATE * AUDIO_CHANNELS * 2))
		&& SUCCEEDED(writer->SetInputMediaType(audiostream, type, nullptr));
	Release(type);
	if (!ok) {
		why = "the encoder refused PCM input";
		Release(writer);
		return(false);
	}

	if (FAILED(writer->BeginWriting())) {
		why = "BeginWriting failed";
		Release(writer);
		return(false);
	}

	std::vector<unsigned int> pixels((size_t)WIDTH * HEIGHT);
	LONGLONG const frameduration = HNS_PER_SECOND / FPS;
	for (int frame = 0; frame < FRAMES && ok; frame++) {
		for (int y = 0; y < HEIGHT; y++) {
			for (int x = 0; x < WIDTH; x++) {
				unsigned int red = (unsigned int)((x * 255) / WIDTH);
				unsigned int green = (unsigned int)((y * 255) / HEIGHT);
				unsigned int blue = (unsigned int)((frame * 255) / FRAMES);
				pixels[(size_t)y * WIDTH + x] = 0xFF000000 | (red << 16) | (green << 8) | blue;
			}
		}
		ok = Write_Sample(writer, videostream, pixels.data(), (DWORD)(pixels.size() * 4), frame * frameduration, frameduration);
	}

	unsigned const audioframes = AUDIO_RATE * FRAMES / FPS;
	std::vector<short> pcm((size_t)audioframes * AUDIO_CHANNELS);
	for (unsigned i = 0; i < audioframes; i++) {
		short value = (short)(std::sin(2.0 * 3.14159265358979 * 440.0 * i / AUDIO_RATE) * 12000.0);
		pcm[(size_t)i * 2] = value;
		pcm[(size_t)i * 2 + 1] = value;
	}
	if (ok) {
		ok = Write_Sample(writer, audiostream, pcm.data(), (DWORD)(pcm.size() * 2), 0, (LONGLONG)audioframes * HNS_PER_SECOND / AUDIO_RATE);
	}

	if (ok) {
		ok = SUCCEEDED(writer->Finalize());
	}
	Release(writer);
	if (!ok) {
		why = "encoding failed";
	}
	return(ok);
}


// Copies the front part of a file, standing in for a download that stopped short.
void Write_Truncated(std::wstring const & source, std::wstring const & path, double fraction)
{
	FILE * in = _wfopen(source.c_str(), L"rb");
	FILE * out = _wfopen(path.c_str(), L"wb");
	if (in != nullptr && out != nullptr) {
		std::fseek(in, 0, SEEK_END);
		long size = std::ftell(in);
		std::fseek(in, 0, SEEK_SET);
		std::vector<char> bytes((size_t)(size * fraction));
		size_t read = std::fread(bytes.data(), 1, bytes.size(), in);
		std::fwrite(bytes.data(), 1, read, out);
	}
	if (in != nullptr) std::fclose(in);
	if (out != nullptr) std::fclose(out);
}


void Write_Junk(std::wstring const & path)
{
	FILE * file = _wfopen(path.c_str(), L"wb");
	if (file != nullptr) {
		unsigned seed = 12345;
		for (int i = 0; i < 4096; i++) {
			seed = seed * 1103515245u + 12345u;
			std::fputc((int)(seed >> 16) & 0xFF, file);
		}
		std::fclose(file);
	}
}

} // namespace


int main(void)
{
	if (!MF_Video_Available()) {
		std::printf("skipped: Media Foundation is not installed on this machine\n");
		return(0);
	}

	if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) || FAILED(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET))) {
		std::printf("skipped: Media Foundation did not start\n");
		return(0);
	}

	std::wstring movie = Temp_Path(L"tiny.mp4");
	std::wstring junk = Temp_Path(L"junk.mp4");
	std::wstring missing = Temp_Path(L"missing.mp4");
	std::wstring truncated = Temp_Path(L"truncated.mp4");
	std::string why;
	bool encoded = Encode_Movie(movie, why);
	MFShutdown();

	if (!encoded) {
		std::printf("skipped: %s\n", why.c_str());
		DeleteFileW(movie.c_str());
		CoUninitialize();
		return(0);
	}

	VideoPlayer * player = Create_MF_Video_Player();
	Check(player != nullptr, "a player is created");
	if (player == nullptr) {
		DeleteFileW(movie.c_str());
		CoUninitialize();
		return(1);
	}

	Check(player->Open(Narrow(movie).c_str()), "the encoded movie opens");
	Check(player->Width() == WIDTH && player->Height() == HEIGHT, "the frame size is the encoded size");
	Check(std::fabs(player->Frame_Rate() - FPS) < 0.01, "the frame rate is the encoded rate");
	Check(player->Has_Audio(), "the sound track is found");
	Check(player->Audio_Channels() == AUDIO_CHANNELS, "the sound track is stereo");
	Check(player->Audio_Rate() > 0, "the sound track has a rate");

	int frames = 0;
	unsigned audioframes = 0;
	double firstframe = -1.0;
	double lastframe = -1.0;
	bool monotonic = true;
	bool sized = true;
	bool endedclean = false;
	VideoPacket packet;
	for (int reads = 0; reads < 10000; reads++) {
		player->Read_Next(packet);
		if (packet.Type == VIDEO_PACKET_FRAME) {
			if (firstframe < 0.0) {
				firstframe = packet.Seconds;
			}
			if (packet.Seconds < lastframe) {
				monotonic = false;
			}
			lastframe = packet.Seconds;
			if (packet.Width != WIDTH || packet.Height != HEIGHT || packet.Bgra.size() != (size_t)WIDTH * HEIGHT * 4) {
				sized = false;
			}
			frames++;
		} else if (packet.Type == VIDEO_PACKET_AUDIO) {
			audioframes += packet.Frames;
		} else {
			endedclean = packet.Type == VIDEO_PACKET_END;
			break;
		}
	}
	Check(frames >= 1, "at least one frame decodes");
	Check(frames <= FRAMES, "no more frames decode than were encoded");
	Check(firstframe == 0.0, "the first frame is at time zero");
	Check(monotonic, "frame times never go backwards");
	Check(sized, "every frame is the frame size in BGRA");
	Check(audioframes > 0, "sound samples decode");
	Check(endedclean, "the movie ends with END rather than an error");
	player->Read_Next(packet);
	Check(packet.Type == VIDEO_PACKET_END, "END repeats after the end");
	player->Close();

	Write_Junk(junk);
	Check(!player->Open(Narrow(junk).c_str()), "a file of random bytes does not open");
	Check(!player->Open(Narrow(missing).c_str()), "a missing file does not open");
	Check(player->Open(Narrow(movie).c_str()), "the movie opens again after a refusal");
	player->Close();

	// A file cut short either refuses to open or runs out cleanly; it never spins.
	Write_Truncated(movie, truncated, 0.6);
	bool truncatedclean = true;
	if (player->Open(Narrow(truncated).c_str())) {
		int reads = 0;
		do {
			player->Read_Next(packet);
			reads++;
		} while (packet.Type != VIDEO_PACKET_END && packet.Type != VIDEO_PACKET_ERROR && reads < 10000);
		truncatedclean = reads < 10000;
		player->Close();
	}
	Check(truncatedclean, "a truncated file ends with END or ERROR");
	delete player;

	DeleteFileW(movie.c_str());
	DeleteFileW(junk.c_str());
	DeleteFileW(truncated.c_str());
	CoUninitialize();

	std::printf("%d failure(s)\n", Failures);
	return(Failures == 0 ? 0 : 1);
}
