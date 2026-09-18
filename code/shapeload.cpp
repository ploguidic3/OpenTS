/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "always.h"

#include "shapeload.h"

#include "cdfile.h"
#include "dbgprint.h"
#include "ini.h"
#include "mixfile.h"
#include "rawfile.h"
#include "shapeset.h"

#include <cctype>
#include <memory>
#include <string>
#include <unordered_map>

namespace {

// The per-frame record that follows the header; ShapeSet keeps its own definition private.
constexpr int ShapeRecordSize = 24;

char const * const PackName = "HDPACK.INI";
char const * const PackSection = "Scale";
char const * const PackUISection = "UI";
char const * const PackUIEntry = "Scale";

struct LooseShape
{
	std::unique_ptr<char[]> Bytes;
	ShapeSource Source;
};

bool OverridesEnabled = true;
unsigned LooseBytes = 0;


std::unordered_map<std::string, LooseShape> & Loose_Registry(void)
{
	static std::unordered_map<std::string, LooseShape> registry;
	return(registry);
}


std::unordered_map<ShapeSet const *, int> & Scale_Index(void)
{
	static std::unordered_map<ShapeSet const *, int> index;
	return(index);
}


std::unordered_map<std::string, std::unordered_map<std::string, int>> & Pack_Index(void)
{
	static std::unordered_map<std::string, std::unordered_map<std::string, int>> index;
	return(index);
}


std::string Upper_Name(char const * name)
{
	std::string key(name);

	for (char & c : key) {
		c = (char)std::toupper((unsigned char)c);
	}

	return(key);
}


std::string Folder_Of(char const * path)
{
	std::string folder(path);
	std::string::size_type const cut = folder.find_last_of("\\/:");

	if (cut == std::string::npos) {
		return(std::string());
	}

	return(folder.substr(0, cut + 1));
}


// The scale HDPACK.INI in a folder assigns to a name, or 0 when it assigns none.
int Pack_Scale(std::string const & folder, std::string const & key)
{
	auto & index = Pack_Index();
	auto found = index.find(folder);

	if (found == index.end()) {
		std::unordered_map<std::string, int> scales;
		std::string const name = folder + PackName;
		RawFileClass file(name.c_str());

		if (file.Is_Available()) {
			INIClass ini;
			ini.Load(file);

			int const count = ini.Entry_Count(PackSection);
			for (int position = 0; position < count; position++) {
				char const * entry = ini.Get_Entry(PackSection, position);
				if (entry != NULL) {
					scales[Upper_Name(entry)] = ini.Get_Int(PackSection, entry, 0);
				}
			}
		}

		found = index.emplace(folder, std::move(scales)).first;
	}

	auto scale = found->second.find(key);
	return(scale == found->second.end() ? 0 : scale->second);
}


int Recognised_Scale(ShapeSet const * loose, std::string & key, std::string const & folder)
{
	int scale = 1;

	ShapeSet const * archived = (ShapeSet const *)MixFileClass::Retrieve(key.data());
	if (archived != NULL && archived->Get_Width() > 0 && archived->Get_Height() > 0
			&& loose->Get_Width() == archived->Get_Width() * 2
			&& loose->Get_Height() == archived->Get_Height() * 2) {
		scale = 2;
	}

	int const assigned = Pack_Scale(folder, key);
	if (assigned > 0) {
		scale = assigned;
	}

	return(scale);
}


LooseShape const & Load_Loose(std::string & key)
{
	auto & registry = Loose_Registry();
	auto found = registry.find(key);

	if (found != registry.end()) {
		return(found->second);
	}

	LooseShape loose;
	CDFileClass file(key.c_str());

	if (file.Is_Available()) {
		int const size = file.Size();

		if (size >= (int)sizeof(ShapeSet)) {
			std::unique_ptr<char[]> bytes(new char[size]);

			file.Open(FileClass::READ);
			int const read = file.Read(bytes.get(), size);
			file.Close();

			ShapeSet const * shape = (ShapeSet const *)bytes.get();
			if (read == size && shape->Get_Count() >= 0 && size >= (int)sizeof(ShapeSet) + shape->Get_Count() * ShapeRecordSize) {
				loose.Source.Shape = shape;
				loose.Source.Scale = Recognised_Scale(shape, key, Folder_Of(file.File_Name()));
				loose.Source.Owned = true;
				loose.Source.Size = size;
				loose.Bytes = std::move(bytes);

				LooseBytes += (unsigned)size;
				Scale_Index()[shape] = loose.Source.Scale;
				DebugString("[ShapeLoad] %s from %s, %d bytes, scale %d, %u loose bytes held\n", key.c_str(), file.File_Name(), size, loose.Source.Scale, LooseBytes);
			} else {
				DebugString("[ShapeLoad] %s is not a shape file, using the archived copy\n", file.File_Name());
			}
		} else {
			DebugString("[ShapeLoad] %s is too short to be a shape file, using the archived copy\n", file.File_Name());
		}
	}

	return(registry.emplace(key, std::move(loose)).first->second);
}


// The interface scale the first HDPACK.INI in the searched folders declares, or 0 for none.
int Read_Pack_UI_Scale(void)
{
	CDFileClass file(PackName);

	if (!file.Is_Available()) {
		return(0);
	}

	INIClass ini;
	ini.Load(file);

	return(ini.Get_Int(PackUISection, PackUIEntry, 0));
}


// The archive lookup writes the name in place, so it is handed the copy this file made.
ShapeSource Archived_Source(std::string & key)
{
	ShapeSource source;
	void * pointer = NULL;
	int size = 0;

	if (MixFileClass::Offset(key.data(), &pointer, NULL, NULL, &size) && pointer != NULL) {
		source.Shape = (ShapeSet const *)pointer;
		source.Size = size;
	}

	return(source);
}

} // namespace


/// <summary>
/// Finds a shape by name. A loose file anywhere in the searched folders is read once and
/// held for the life of the process, so the pointer stays valid like an archived one; the
/// archives answer when there is no loose file or overrides are off. Shape is NULL when
/// neither has the name.
/// </summary>
ShapeSource Fetch_Shape_Source(char const * name)
{
	if (name == NULL) {
		return(ShapeSource());
	}

	std::string key = Upper_Name(name);

	if (OverridesEnabled) {
		LooseShape const & loose = Load_Loose(key);
		if (loose.Source.Shape != NULL) {
			return(loose.Source);
		}
	}

	return(Archived_Source(key));
}


ShapeSet const * Fetch_Shape(char const * name)
{
	return(Fetch_Shape_Source(name).Shape);
}


int Pack_UI_Scale(void)
{
	static int _scale = -1;

	if (!OverridesEnabled) {
		return(1);
	}

	if (_scale < 0) {
		_scale = Read_Pack_UI_Scale();
		_scale = _scale > 0 ? _scale : 1;
		DebugString("[ShapeLoad] HD pack declares interface scale %d\n", _scale);
	}

	return(_scale);
}


int Shape_Scale(ShapeSet const * shape)
{
	auto & index = Scale_Index();
	auto found = index.find(shape);

	return(found == index.end() ? 1 : found->second);
}


void Enable_Shape_Overrides(bool enabled)
{
	OverridesEnabled = enabled;
}


bool Shape_Overrides_Enabled(void)
{
	return(OverridesEnabled);
}


unsigned Shape_Override_Bytes(void)
{
	return(LooseBytes);
}
