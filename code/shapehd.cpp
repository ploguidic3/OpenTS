/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "always.h"

#include "shapehd.h"

#include "dbgprint.h"
#include "shapeset.h"

#include <cstring>
#include <map>
#include <memory>
#include <vector>

namespace {

constexpr int HeaderSize = 8;
constexpr int RecordSize = 24;

// Where the offset of a frame's pixels sits inside its record; the four fields before it are
// followed by the color and the unused bytes.
constexpr int DataOffset = 20;

constexpr int FlagRLE = 0x02;

// A frame's data size and a row's length prefix are both held in sixteen bits.
constexpr int FrameSizeLimit = 0x7FFF;
constexpr int RowSizeLimit = 0xFFFF;

std::map<std::pair<ShapeSet const *, int>, std::unique_ptr<char[]>> & Registry(void)
{
	static std::map<std::pair<ShapeSet const *, int>, std::unique_ptr<char[]>> registry;
	return(registry);
}

unsigned HeldBytes = 0;


short Read_Short(char const * bytes, int offset)
{
	short value = 0;
	std::memcpy(&value, bytes + offset, sizeof(value));
	return(value);
}


void Write_Short(char * bytes, int offset, int value)
{
	short const written = (short)value;
	std::memcpy(bytes + offset, &written, sizeof(written));
}


void Write_Int(char * bytes, int offset, int value)
{
	std::memcpy(bytes + offset, &value, sizeof(value));
}


// Rows of an encoded frame carry their own byte length, so one row is read at a time.
bool Decode_RLE_Row(unsigned char const *& source, unsigned char const * end, unsigned char * row, int width)
{
	if (source + sizeof(unsigned short) > end) {
		return(false);
	}

	unsigned short length = 0;
	std::memcpy(&length, source, sizeof(length));

	if (length < sizeof(unsigned short) || source + length > end) {
		return(false);
	}

	unsigned char const * pixel = source + sizeof(unsigned short);
	unsigned char const * const stop = source + length;
	int written = 0;

	while (pixel < stop && written < width) {
		if (*pixel == '\0') {
			if (pixel + 1 >= stop) {
				return(false);
			}
			int run = pixel[1];
			if (run > width - written) {
				run = width - written;
			}
			std::memset(row + written, 0, run);
			written += run;
			pixel += 2;
		} else {
			row[written++] = *pixel++;
		}
	}

	std::memset(row + written, 0, width - written);
	source += length;
	return(true);
}


int Encode_RLE_Row(unsigned char const * row, int width, unsigned char * out)
{
	int length = (int)sizeof(unsigned short);
	int index = 0;

	while (index < width) {
		if (row[index] == '\0') {
			int run = 0;
			while (index + run < width && row[index + run] == '\0' && run < 255) {
				run++;
			}
			if (out != NULL) {
				out[length] = '\0';
				out[length + 1] = (unsigned char)run;
			}
			length += 2;
			index += run;
		} else {
			if (out != NULL) {
				out[length] = row[index];
			}
			length++;
			index++;
		}
	}

	if (out != NULL) {
		unsigned short const written = (unsigned short)length;
		std::memcpy(out, &written, sizeof(written));
	}
	return(length);
}


bool Decode_Frame(char const * shape, int offset, int size, int width, int height, bool rle, std::vector<unsigned char> & pixels)
{
	pixels.assign((std::size_t)width * height, 0);

	if (width <= 0 || height <= 0) {
		return(true);
	}

	unsigned char const * source = (unsigned char const *)shape + offset;

	if (!rle) {
		std::memcpy(pixels.data(), source, (std::size_t)width * height);
		return(true);
	}

	unsigned char const * end = source + size;
	for (int y = 0; y < height; y++) {
		if (!Decode_RLE_Row(source, end, pixels.data() + (std::size_t)y * width, width)) {
			return(false);
		}
	}
	return(true);
}


void Magnify(std::vector<unsigned char> const & source, int width, int height, int factor, std::vector<unsigned char> & dest)
{
	dest.assign((std::size_t)width * factor * height * factor, 0);

	for (int y = 0; y < height * factor; y++) {
		unsigned char const * in = source.data() + (std::size_t)(y / factor) * width;
		unsigned char * out = dest.data() + (std::size_t)y * width * factor;

		for (int x = 0; x < width * factor; x++) {
			out[x] = in[x / factor];
		}
	}
}


int Encoded_Size(std::vector<unsigned char> const & pixels, int width, int height, bool rle, unsigned char * out)
{
	if (!rle) {
		if (out != NULL) {
			std::memcpy(out, pixels.data(), (std::size_t)width * height);
		}
		return(width * height);
	}

	int total = 0;
	for (int y = 0; y < height; y++) {
		unsigned char const * row = pixels.data() + (std::size_t)y * width;
		int const length = Encode_RLE_Row(row, width, out != NULL ? out + total : NULL);
		if (length > RowSizeLimit) {
			return(-1);
		}
		total += length;
	}
	return(total);
}


// A frame the record fields cannot describe leaves the whole shape unenlarged.
bool Frame_Fits(int size)
{
	return(size >= 0 && size <= FrameSizeLimit);
}


std::unique_ptr<char[]> Build(ShapeSet const * shape, int factor, int & bytes)
{
	char const * source = (char const *)shape;
	int const count = shape->Get_Count();

	std::vector<std::vector<unsigned char>> encoded((std::size_t)count);
	std::vector<int> sizes((std::size_t)count, 0);

	std::vector<unsigned char> pixels;
	std::vector<unsigned char> grown;

	for (int index = 0; index < count; index++) {
		int const record = HeaderSize + RecordSize * index;
		int const width = Read_Short(source, record + 4);
		int const height = Read_Short(source, record + 6);
		int const flags = Read_Short(source, record + 8);
		int const size = (unsigned short)Read_Short(source, record + 10);
		int data = 0;
		std::memcpy(&data, source + record + DataOffset, sizeof(data));

		if (data == 0 || width <= 0 || height <= 0) {
			continue;
		}

		bool const rle = (flags & FlagRLE) != 0;
		if (!Decode_Frame(source, data, size, width, height, rle, pixels)) {
			return(nullptr);
		}

		Magnify(pixels, width, height, factor, grown);

		int const length = Encoded_Size(grown, width * factor, height * factor, rle, NULL);
		if (!Frame_Fits(length)) {
			return(nullptr);
		}

		encoded[index].resize((std::size_t)length);
		Encoded_Size(grown, width * factor, height * factor, rle, encoded[index].data());
		sizes[index] = length;
	}

	int total = HeaderSize + RecordSize * count;
	for (int index = 0; index < count; index++) {
		total += sizes[index];
	}

	std::unique_ptr<char[]> buffer(new char[total]);
	std::memcpy(buffer.get(), source, HeaderSize + RecordSize * count);

	Write_Short(buffer.get(), 2, shape->Get_Width() * factor);
	Write_Short(buffer.get(), 4, shape->Get_Height() * factor);

	int offset = HeaderSize + RecordSize * count;
	for (int index = 0; index < count; index++) {
		int const record = HeaderSize + RecordSize * index;

		if (sizes[index] == 0) {
			Write_Int(buffer.get(), record + DataOffset, 0);
			continue;
		}

		Write_Short(buffer.get(), record + 0, Read_Short(source, record + 0) * factor);
		Write_Short(buffer.get(), record + 2, Read_Short(source, record + 2) * factor);
		Write_Short(buffer.get(), record + 4, Read_Short(source, record + 4) * factor);
		Write_Short(buffer.get(), record + 6, Read_Short(source, record + 6) * factor);
		Write_Short(buffer.get(), record + 10, sizes[index]);
		Write_Int(buffer.get(), record + DataOffset, offset);

		std::memcpy(buffer.get() + offset, encoded[index].data(), (std::size_t)sizes[index]);
		offset += sizes[index];
	}

	bytes = total;
	return(buffer);
}

} // namespace


/// <summary>
/// Enlarges classic artwork to the scale the rest of the interface is drawn at. The returned
/// shape carries the same frame count, flags and encoding as the original, with every offset
/// and dimension multiplied, so it draws through the same blitters. The original is returned
/// when the factor is one or a frame grows past what its record can describe.
/// </summary>
ShapeSet const * Scaled_Shape(ShapeSet const * shape, int factor)
{
	if (shape == NULL || factor <= 1 || shape->Get_Count() <= 0) {
		return(shape);
	}

	auto & registry = Registry();
	auto const key = std::make_pair(shape, factor);
	auto found = registry.find(key);

	if (found != registry.end()) {
		return(found->second == nullptr ? shape : (ShapeSet const *)found->second.get());
	}

	int bytes = 0;
	std::unique_ptr<char[]> built = Build(shape, factor, bytes);

	if (built == nullptr) {
		DebugString("[ShapeHD] a %dx%d shape could not be enlarged by %d and is drawn as it is\n", shape->Get_Width(), shape->Get_Height(), factor);
		registry.emplace(key, nullptr);
		return(shape);
	}

	HeldBytes += (unsigned)bytes;
	ShapeSet const * result = (ShapeSet const *)built.get();
	registry.emplace(key, std::move(built));
	return(result);
}


void Scaled_Shape_Reset(void)
{
	Registry().clear();
	HeldBytes = 0;
}


unsigned Scaled_Shape_Bytes(void)
{
	return(HeldBytes);
}
