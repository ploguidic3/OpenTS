/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// Exercises the shape enlarger on shapes the harness writes itself: what the header and the
// frame records say afterwards, what the pixels of a plain and of a compressed frame become,
// and when the original is handed back instead.

#include <cstdio>
#include <cstring>
#include <vector>

#include "shapehd.h"
#include "shapeset.h"

namespace {

int Failures = 0;

constexpr int HeaderSize = 8;
constexpr int RecordSize = 24;
constexpr int DataOffset = 20;

constexpr int FlagTransparent = 0x01;
constexpr int FlagRLE = 0x02;


void Check(bool condition, char const * what)
{
	std::printf("%-62s %s\n", what, condition ? "ok" : "FAILED");

	if (!condition) {
		Failures++;
	}
}


void Check_Value(int actual, int expected, char const * what)
{
	Check(actual == expected, what);
	if (actual != expected) {
		std::printf("    got %d, expected %d\n", actual, expected);
	}
}


void Put_Short(std::vector<char> & bytes, int value)
{
	bytes.push_back((char)(value & 0xFF));
	bytes.push_back((char)((value >> 8) & 0xFF));
}


void Put_Int(std::vector<char> & bytes, int value)
{
	Put_Short(bytes, value & 0xFFFF);
	Put_Short(bytes, (value >> 16) & 0xFFFF);
}


short Get_Short(char const * bytes, int offset)
{
	short value = 0;
	std::memcpy(&value, bytes + offset, sizeof(value));
	return(value);
}


int Get_Int(char const * bytes, int offset)
{
	int value = 0;
	std::memcpy(&value, bytes + offset, sizeof(value));
	return(value);
}


struct Frame
{
	int X = 0;
	int Y = 0;
	int Width = 0;
	int Height = 0;
	int Flags = 0;
	std::vector<unsigned char> Pixels;
};


std::vector<unsigned char> Encode_Row(unsigned char const * row, int width)
{
	std::vector<unsigned char> body;
	int index = 0;

	while (index < width) {
		if (row[index] == 0) {
			int run = 0;
			while (index + run < width && row[index + run] == 0 && run < 255) {
				run++;
			}
			body.push_back(0);
			body.push_back((unsigned char)run);
			index += run;
		} else {
			body.push_back(row[index]);
			index++;
		}
	}

	std::vector<unsigned char> out;
	int const length = (int)body.size() + 2;
	out.push_back((unsigned char)(length & 0xFF));
	out.push_back((unsigned char)((length >> 8) & 0xFF));
	out.insert(out.end(), body.begin(), body.end());
	return(out);
}


std::vector<unsigned char> Encode_Frame(Frame const & frame)
{
	if ((frame.Flags & FlagRLE) == 0) {
		return(frame.Pixels);
	}

	std::vector<unsigned char> out;
	for (int y = 0; y < frame.Height; y++) {
		std::vector<unsigned char> row = Encode_Row(frame.Pixels.data() + (std::size_t)y * frame.Width, frame.Width);
		out.insert(out.end(), row.begin(), row.end());
	}
	return(out);
}


// The layout shapeset.h reads: the header, one record per frame, then each frame's data.
std::vector<char> Make_Shape(int width, int height, std::vector<Frame> const & frames)
{
	std::vector<char> bytes;
	int const count = (int)frames.size();

	Put_Short(bytes, 0);
	Put_Short(bytes, width);
	Put_Short(bytes, height);
	Put_Short(bytes, count);

	std::vector<std::vector<unsigned char>> data;
	for (Frame const & frame : frames) {
		data.push_back(Encode_Frame(frame));
	}

	int offset = HeaderSize + RecordSize * count;
	for (int index = 0; index < count; index++) {
		Frame const & frame = frames[index];

		Put_Short(bytes, frame.X);
		Put_Short(bytes, frame.Y);
		Put_Short(bytes, frame.Width);
		Put_Short(bytes, frame.Height);
		Put_Short(bytes, frame.Flags);
		Put_Short(bytes, (int)data[index].size());
		bytes.push_back((char)(index + 1));
		bytes.push_back((char)(index + 2));
		bytes.push_back((char)(index + 3));
		for (int unused = 0; unused < 5; unused++) {
			bytes.push_back(0);
		}
		Put_Int(bytes, offset);
		offset += (int)data[index].size();
	}

	for (std::vector<unsigned char> const & frame : data) {
		bytes.insert(bytes.end(), frame.begin(), frame.end());
	}

	return(bytes);
}


std::vector<unsigned char> Decode_Frame(char const * shape, int index)
{
	int const record = HeaderSize + RecordSize * index;
	int const width = Get_Short(shape, record + 4);
	int const height = Get_Short(shape, record + 6);
	int const flags = Get_Short(shape, record + 8);
	unsigned char const * source = (unsigned char const *)shape + Get_Int(shape, record + DataOffset);

	std::vector<unsigned char> pixels((std::size_t)width * height, 0);

	if ((flags & FlagRLE) == 0) {
		std::memcpy(pixels.data(), source, pixels.size());
		return(pixels);
	}

	for (int y = 0; y < height; y++) {
		unsigned short length = 0;
		std::memcpy(&length, source, sizeof(length));

		unsigned char const * pixel = source + sizeof(unsigned short);
		unsigned char const * const stop = source + length;
		int written = 0;

		while (pixel < stop && written < width) {
			if (*pixel == 0) {
				written += pixel[1];
				pixel += 2;
			} else {
				pixels[(std::size_t)y * width + written] = *pixel;
				written++;
				pixel++;
			}
		}
		source += length;
	}

	return(pixels);
}


Frame Make_Frame(int x, int y, int width, int height, int flags, unsigned char first)
{
	Frame frame;
	frame.X = x;
	frame.Y = y;
	frame.Width = width;
	frame.Height = height;
	frame.Flags = flags;
	frame.Pixels.assign((std::size_t)width * height, 0);

	for (int index = 0; index < width * height; index++) {
		frame.Pixels[index] = (unsigned char)((index % 2) == 0 ? first + index : 0);
	}

	return(frame);
}


bool Is_Magnified(std::vector<unsigned char> const & source, int width, int height, std::vector<unsigned char> const & grown, int factor)
{
	for (int y = 0; y < height * factor; y++) {
		for (int x = 0; x < width * factor; x++) {
			if (grown[(std::size_t)y * width * factor + x] != source[(std::size_t)(y / factor) * width + (x / factor)]) {
				return(false);
			}
		}
	}
	return(true);
}


void Test_Plain_And_Compressed(void)
{
	std::vector<Frame> frames;
	frames.push_back(Make_Frame(1, 2, 3, 2, FlagTransparent, 10));
	frames.push_back(Make_Frame(0, 1, 5, 3, FlagTransparent | FlagRLE, 40));

	std::vector<char> bytes = Make_Shape(8, 6, frames);
	ShapeSet const * shape = (ShapeSet const *)bytes.data();

	ShapeSet const * grown = Scaled_Shape(shape, 2);
	char const * raw = (char const *)grown;

	Check(grown != shape, "an enlarged shape is a copy, not the original");
	Check_Value(grown->Get_Count(), 2, "the frame count is unchanged");
	Check_Value(grown->Get_Width(), 16, "the logical width is doubled");
	Check_Value(grown->Get_Height(), 12, "the logical height is doubled");

	Check_Value(Get_Short(raw, HeaderSize + 0), 2, "the first frame's X offset is doubled");
	Check_Value(Get_Short(raw, HeaderSize + 2), 4, "the first frame's Y offset is doubled");
	Check_Value(Get_Short(raw, HeaderSize + 4), 6, "the first frame's width is doubled");
	Check_Value(Get_Short(raw, HeaderSize + 6), 4, "the first frame's height is doubled");
	Check_Value(Get_Short(raw, HeaderSize + 8), FlagTransparent, "a plain frame stays plain");
	Check_Value(Get_Short(raw, HeaderSize + RecordSize + 8), FlagTransparent | FlagRLE, "a compressed frame stays compressed");
	Check_Value((unsigned char)raw[HeaderSize + 12], 1, "the frame's stand-in color is kept");

	Check(Is_Magnified(frames[0].Pixels, 3, 2, Decode_Frame(raw, 0), 2), "a plain frame's pixels are grown to squares");
	Check(Is_Magnified(frames[1].Pixels, 5, 3, Decode_Frame(raw, 1), 2), "a compressed frame's pixels are grown to squares");

	Check(Scaled_Shape(shape, 2) == grown, "the enlarged copy is made once and held");
	Check(Scaled_Shape(shape, 1) == shape, "a factor of one hands back the original");
	Check(Scaled_Shape(NULL, 2) == NULL, "nothing to enlarge stays nothing");
	Check(Scaled_Shape_Bytes() > 0, "the held bytes are counted");

	Scaled_Shape_Reset();
	Check_Value((int)Scaled_Shape_Bytes(), 0, "a reset drops what was held");
}


void Test_Frame_Too_Large(void)
{
	std::vector<Frame> frames;
	frames.push_back(Make_Frame(0, 0, 100, 100, 0, 1));

	std::vector<char> bytes = Make_Shape(100, 100, frames);
	ShapeSet const * shape = (ShapeSet const *)bytes.data();

	Check(Scaled_Shape(shape, 4) == shape, "a frame that outgrows its record is left alone");
	Check_Value((int)Scaled_Shape_Bytes(), 0, "nothing is held for a shape that was left alone");

	Scaled_Shape_Reset();
}

} // namespace


int main(void)
{
	Test_Plain_And_Compressed();
	Test_Frame_Too_Large();

	std::printf("\n%d failure(s)\n", Failures);
	return(Failures == 0 ? 0 : 1);
}
