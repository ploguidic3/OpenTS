/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "always.h"

#include "fontload.h"

#include "cdfile.h"
#include "dbgprint.h"
#include "mixfile.h"
#include "shapeload.h"

#include <cctype>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>

namespace {

// The header fields wwfont.cpp reads: where the information block starts, and the tallest
// glyph within it.
constexpr int InfoBlockOffsetField = 4;
constexpr int InfoBlockMaxHeight = 4;
constexpr int SmallestFont = 16;

struct LooseFont
{
	std::unique_ptr<char[]> Bytes;
	FontSource Source;
};

unsigned LooseBytes = 0;


std::unordered_map<std::string, LooseFont> & Loose_Registry(void)
{
	static std::unordered_map<std::string, LooseFont> registry;
	return(registry);
}


std::string Upper_Name(char const * name)
{
	std::string key(name);

	for (char & c : key) {
		c = (char)std::toupper((unsigned char)c);
	}

	return(key);
}


int Font_Height(void const * data, int size)
{
	if (data == NULL || size < SmallestFont) {
		return(0);
	}

	unsigned char const * bytes = (unsigned char const *)data;
	unsigned short info = 0;
	std::memcpy(&info, bytes + InfoBlockOffsetField, sizeof(info));

	if ((int)info + InfoBlockMaxHeight >= size) {
		return(0);
	}

	return(bytes[info + InfoBlockMaxHeight]);
}


// A pack that declares an interface scale gives its fonts the same one when the file really
// is that much taller than the archived font.
int Recognised_Scale(void const * loose, int loosesize, std::string & key)
{
	int const pack = Pack_UI_Scale();
	if (pack <= 1) {
		return(1);
	}

	int const height = Font_Height(loose, loosesize);
	int archivedheight = 0;

	void * pointer = NULL;
	int size = 0;
	if (MixFileClass::Offset(key.data(), &pointer, NULL, NULL, &size)) {
		archivedheight = Font_Height(pointer, size);
	}

	if (archivedheight > 0 && height == archivedheight * pack) {
		return(pack);
	}

	return(1);
}


LooseFont const & Load_Loose(std::string & key)
{
	auto & registry = Loose_Registry();
	auto found = registry.find(key);

	if (found != registry.end()) {
		return(found->second);
	}

	LooseFont loose;
	CDFileClass file(key.c_str());

	if (file.Is_Available()) {
		int const size = file.Size();

		if (size >= SmallestFont) {
			std::unique_ptr<char[]> bytes(new char[size]);

			file.Open(FileClass::READ);
			int const read = file.Read(bytes.get(), size);
			file.Close();

			if (read == size && Font_Height(bytes.get(), size) > 0) {
				loose.Source.Data = bytes.get();
				loose.Source.Scale = Recognised_Scale(bytes.get(), size, key);
				loose.Source.Owned = true;
				loose.Source.Size = size;
				loose.Bytes = std::move(bytes);

				LooseBytes += (unsigned)size;
				DebugString("[FontLoad] %s from %s, %d bytes, scale %d, %u loose bytes held\n", key.c_str(), file.File_Name(), size, loose.Source.Scale, LooseBytes);
			} else {
				DebugString("[FontLoad] %s is not a font file, using the archived copy\n", file.File_Name());
			}
		} else {
			DebugString("[FontLoad] %s is too short to be a font file, using the archived copy\n", file.File_Name());
		}
	}

	return(registry.emplace(key, std::move(loose)).first->second);
}

} // namespace


/// <summary>
/// Finds a font by name. A loose file in the searched folders is read once and held for the
/// life of the process, so the pointer stays valid like an archived one; the archives answer
/// when there is no loose file or the shape overrides are switched off. A loose font counts
/// as an enlarged one only where it is as much taller than the archived font as the pack
/// declares, so a replacement at the classic size still prints as before.
/// </summary>
FontSource Fetch_Font_Source(char const * name)
{
	FontSource source;

	if (name == NULL) {
		return(source);
	}

	std::string key = Upper_Name(name);

	if (Shape_Overrides_Enabled()) {
		LooseFont const & loose = Load_Loose(key);
		if (loose.Source.Data != NULL) {
			return(loose.Source);
		}
	}

	source.Data = MixFileClass::Retrieve(key.data());
	return(source);
}


unsigned Font_Override_Bytes(void)
{
	return(LooseBytes);
}
