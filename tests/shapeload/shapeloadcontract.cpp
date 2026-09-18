/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

// Exercises the shape loader without the engine or any game data: which copy of a shape
// answers, how a loose file's scale is recognised, and what the AssetOverrides switch
// changes. The archives are stood in for by a table this harness fills, and every file it
// uses is one it makes itself.

#include <windows.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "cdfile.h"
#include "mixfile.h"
#include "rawfile.h"
#include "shapeload.h"
#include "shapeset.h"

namespace {

int Failures = 0;

std::string Root;
char OriginalDirectory[MAX_PATH];

std::map<std::string, std::vector<char>> Archived;


void Check(bool condition, char const * what)
{
	std::printf("%-62s %s\n", what, condition ? "ok" : "FAILED");

	if (!condition) {
		Failures++;
	}
}


std::string Upper(char const * name)
{
	std::string key(name);
	for (char & c : key) {
		c = (char)toupper((unsigned char)c);
	}
	return(key);
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


// A two frame shape in the layout shapeset.h reads: the header, one record per frame, then
// the pixels. Each frame is two by two pixels filled with the fill byte.
std::vector<char> Make_Shape(int width, int height, char fill)
{
	int const count = 2;
	int const record = 24;
	int const frame = 2 * 2;
	std::vector<char> bytes;

	Put_Short(bytes, 0);
	Put_Short(bytes, width);
	Put_Short(bytes, height);
	Put_Short(bytes, count);

	for (int index = 0; index < count; index++) {
		Put_Short(bytes, 0);
		Put_Short(bytes, 0);
		Put_Short(bytes, 2);
		Put_Short(bytes, 2);
		Put_Short(bytes, 0);
		Put_Short(bytes, frame);
		bytes.push_back(fill);
		bytes.push_back(fill);
		bytes.push_back(fill);
		for (int unused = 0; unused < 5; unused++) {
			bytes.push_back(0);
		}
		Put_Int(bytes, 8 + count * record + index * frame);
	}

	for (int index = 0; index < count * frame; index++) {
		bytes.push_back(fill);
	}

	return(bytes);
}


void Write_Bytes(std::string const & path, std::vector<char> const & bytes)
{
	RawFileClass file(path.c_str());

	file.Open(FileClass::WRITE);
	if (!bytes.empty()) {
		file.Write(bytes.data(), (int)bytes.size());
	}
	file.Close();
}


void Write_Text(std::string const & path, char const * contents)
{
	std::vector<char> bytes(contents, contents + strlen(contents));
	Write_Bytes(path, bytes);
}


void Register_Archived(char const * name, std::vector<char> const & bytes)
{
	Archived[Upper(name)] = bytes;
}


ShapeSet const * Archived_Shape(char const * name)
{
	return((ShapeSet const *)Archived[Upper(name)].data());
}


bool Same_Bytes(ShapeSet const * shape, std::vector<char> const & bytes)
{
	return(shape != NULL && memcmp(shape, bytes.data(), bytes.size()) == 0);
}


void Make_Directory(std::string const & path)
{
	CreateDirectory(path.c_str(), NULL);
}


/*
 * The harness works inside a tree of its own, with the current directory at its root so
 * that an unnamed directory means the root, as it means the game's own directory in play,
 * and an HD folder searched after it, as a deployment's OPENTS.INI would name.
 */
bool Make_Root(void)
{
	char temp[MAX_PATH];
	if (GetTempPath(sizeof(temp), temp) == 0) {
		return(false);
	}

	char name[MAX_PATH];
	std::snprintf(name, sizeof(name), "%sopents-shapeload-%lu", temp, GetCurrentProcessId());
	Root = name;

	Make_Directory(Root);
	Make_Directory(Root + "\\HD");

	// The pack file is read once per folder, on the first loose file found there, so it is
	// in place before any test fetches.
	Write_Text(Root + "\\HD\\HDPACK.INI", "[UI]\nScale=2\n\n[Scale]\npacked.shp=2\nPACKED3.SHP=3\nOTHER.SHP=2\n");

	if (SetCurrentDirectory(Root.c_str()) == 0) {
		return(false);
	}

	CDFileClass::Add_Search_Drive("HD\\");
	return(true);
}


void Remove_Root(void)
{
	SetCurrentDirectory(OriginalDirectory);

	// The tree is shallow and entirely this harness's own, so it is removed by name.
	char command[MAX_PATH + 32];
	std::snprintf(command, sizeof(command), "cmd /c rd /s /q \"%s\"", Root.c_str());
	system(command);
}


void Test_A_Loose_File_Answers(void)
{
	std::vector<char> const bytes = Make_Shape(48, 48, 1);
	Write_Bytes(Root + "\\HD\\LOOSE.SHP", bytes);

	ShapeSource const source = Fetch_Shape_Source("LOOSE.SHP");
	Check(Same_Bytes(source.Shape, bytes), "a loose file in a searched folder is the shape returned");
	Check(source.Owned, "the loose copy is the loader's own");
	Check(source.Size == (int)bytes.size(), "its size is the file's size");
	Check(source.Scale == 1 && Shape_Scale(source.Shape) == 1, "with no archived copy to compare it is 1x art");

	Check(Fetch_Shape("LOOSE.SHP") == source.Shape, "asking again returns the same copy");
	Check(Fetch_Shape("loose.shp") == source.Shape, "the name is matched without regard to case");
	Check(Shape_Override_Bytes() == bytes.size(), "the bytes held are accounted for");
}


void Test_Twice_The_Size_Is_Two_Times_Art(void)
{
	std::vector<char> const archived = Make_Shape(24, 24, 2);
	std::vector<char> const loose = Make_Shape(48, 48, 3);
	Register_Archived("DOUBLE.SHP", archived);
	Write_Bytes(Root + "\\HD\\DOUBLE.SHP", loose);

	ShapeSource const source = Fetch_Shape_Source("DOUBLE.SHP");
	Check(Same_Bytes(source.Shape, loose), "a loose file wins over the archived copy");
	Check(source.Scale == 2, "a loose shape twice the archived one's size is 2x art");
	Check(Shape_Scale(source.Shape) == 2, "the scale is found from the shape again");
	Check(Shape_Scale(Archived_Shape("DOUBLE.SHP")) == 1, "the archived copy stays 1x art");

	std::vector<char> const same = Make_Shape(24, 24, 4);
	Register_Archived("SAME.SHP", archived);
	Write_Bytes(Root + "\\HD\\SAME.SHP", same);
	Check(Fetch_Shape_Source("SAME.SHP").Scale == 1, "a loose shape the archived one's size is 1x art");

	std::vector<char> const wide = Make_Shape(48, 24, 5);
	Register_Archived("WIDE.SHP", archived);
	Write_Bytes(Root + "\\HD\\WIDE.SHP", wide);
	Check(Fetch_Shape_Source("WIDE.SHP").Scale == 1, "twice the width alone is not 2x art");
}


void Test_The_Pack_File_States_The_Scale(void)
{
	Write_Bytes(Root + "\\HD\\PACKED.SHP", Make_Shape(48, 48, 6));
	Check(Fetch_Shape_Source("PACKED.SHP").Scale == 2, "HDPACK.INI in the file's folder assigns a scale");

	Register_Archived("PACKED3.SHP", Make_Shape(24, 24, 7));
	Write_Bytes(Root + "\\HD\\PACKED3.SHP", Make_Shape(48, 48, 8));
	Check(Fetch_Shape_Source("PACKED3.SHP").Scale == 3, "an assigned scale wins over the size comparison");

	// A pack file in another folder says nothing about a file found beside the game.
	Write_Bytes(Root + "\\OTHER.SHP", Make_Shape(48, 48, 9));
	Check(Fetch_Shape_Source("OTHER.SHP").Scale == 1, "the pack file speaks only for its own folder");

	Check(Pack_UI_Scale() == 2, "the pack states the scale it draws the interface at");
}


void Test_The_Archives_Answer_Otherwise(void)
{
	std::vector<char> const archived = Make_Shape(24, 24, 10);
	Register_Archived("ARCHIVED.SHP", archived);

	ShapeSource const source = Fetch_Shape_Source("ARCHIVED.SHP");
	Check(source.Shape == Archived_Shape("ARCHIVED.SHP"), "with no loose file the archived copy is returned");
	Check(!source.Owned, "the archived copy is not the loader's own");
	Check(source.Size == (int)archived.size() && source.Scale == 1, "its size and scale are the archive's");
	Check(Fetch_Shape("ARCHIVED.SHP") == source.Shape, "the plain lookup agrees");

	Check(Fetch_Shape("MISSING.SHP") == NULL, "a name nobody holds is NULL");
	Check(Fetch_Shape_Source("MISSING.SHP").Shape == NULL, "and has no source");
	Check(Fetch_Shape(NULL) == NULL, "no name at all is NULL");
}


void Test_The_Switch_Turns_Loose_Files_Off(void)
{
	std::vector<char> const loose = Make_Shape(48, 48, 11);
	Register_Archived("BOTH.SHP", Make_Shape(24, 24, 12));
	Write_Bytes(Root + "\\HD\\BOTH.SHP", loose);

	Write_Bytes(Root + "\\HD\\ONLYLOOSE.SHP", Make_Shape(48, 48, 18));

	Enable_Shape_Overrides(false);
	Check(!Shape_Overrides_Enabled(), "the switch reports itself off");
	Check(Fetch_Shape("BOTH.SHP") == Archived_Shape("BOTH.SHP"), "with overrides off the archived copy answers");
	Check(Fetch_Shape("ONLYLOOSE.SHP") == NULL, "with overrides off a loose file alone is not found");
	Check(Pack_UI_Scale() == 1, "with overrides off the pack's interface scale is not taken");

	Enable_Shape_Overrides(true);
	Check(Same_Bytes(Fetch_Shape("BOTH.SHP"), loose), "with overrides on again the loose file answers");
}


void Test_A_File_That_Is_Not_A_Shape_Is_Passed_Over(void)
{
	unsigned const held = Shape_Override_Bytes();

	Register_Archived("SHORT.SHP", Make_Shape(24, 24, 13));
	Write_Text(Root + "\\HD\\SHORT.SHP", "abc");
	Check(Fetch_Shape("SHORT.SHP") == Archived_Shape("SHORT.SHP"), "a file shorter than the header is passed over");

	std::vector<char> truncated = Make_Shape(24, 24, 14);
	truncated.resize(20);
	Register_Archived("TRUNC.SHP", Make_Shape(24, 24, 15));
	Write_Bytes(Root + "\\HD\\TRUNC.SHP", truncated);
	Check(Fetch_Shape("TRUNC.SHP") == Archived_Shape("TRUNC.SHP"), "a file cut off inside its records is passed over");

	Check(Shape_Override_Bytes() == held, "a passed over file is not held");
}


void Test_The_Search_Order_Is_The_File_Layer_Order(void)
{
	std::vector<char> const beside = Make_Shape(48, 48, 16);
	std::vector<char> const folder = Make_Shape(48, 48, 17);
	Write_Bytes(Root + "\\NEAR.SHP", beside);
	Write_Bytes(Root + "\\HD\\NEAR.SHP", folder);

	Check(Same_Bytes(Fetch_Shape("NEAR.SHP"), beside), "a copy beside the game wins over one in a searched folder");
}

} // namespace


/*
 * The archives, stood in for. Retrieve answers from the table the tests fill, and Offset
 * reports the size beside it, as the cached archives do.
 */
void const * MixFileClass::Retrieve(char const * filename)
{
	auto found = Archived.find(Upper(filename));
	return(found == Archived.end() ? NULL : found->second.data());
}


bool MixFileClass::Offset(char const * filename, void ** realptr, MixFileClass ** mixfile, int * offset, int * size)
{
	auto found = Archived.find(Upper(filename));
	if (found == Archived.end()) {
		return(false);
	}

	if (realptr != NULL) *realptr = found->second.data();
	if (mixfile != NULL) *mixfile = NULL;
	if (offset != NULL) *offset = 0;
	if (size != NULL) *size = (int)found->second.size();
	return(true);
}


int main(void)
{
	GetCurrentDirectory(sizeof(OriginalDirectory), OriginalDirectory);

	if (!Make_Root()) {
		std::printf("could not create the working directory\n");
		return(1);
	}

	std::printf("Working in %s\n\n", Root.c_str());

	Test_A_Loose_File_Answers();
	Test_Twice_The_Size_Is_Two_Times_Art();
	Test_The_Pack_File_States_The_Scale();
	Test_The_Archives_Answer_Otherwise();
	Test_The_Switch_Turns_Loose_Files_Off();
	Test_A_File_That_Is_Not_A_Shape_Is_Passed_Over();
	Test_The_Search_Order_Is_The_File_Layer_Order();

	CDFileClass::Clear_Search_Drives();
	Remove_Root();

	std::printf("\n%s\n", Failures == 0 ? "All checks passed." : "There were failures.");
	return(Failures == 0 ? 0 : 1);
}
