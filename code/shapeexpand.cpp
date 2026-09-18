/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 OpenTS contributors
 *
 * See LICENSE.md for applicable additional terms and warranty disclaimers.
 ******************************************************************************/

#include "shapeexpand.h"

#include "assetscale.h"
#include "scaleblit.h"
#include "shapeload.h"
#include "shapeset.h"

#include <list>
#include <unordered_map>
#include <vector>


namespace
{

struct FrameKey
{
	ShapeSet const * Shape;
	int Index;
	int Factor;

	bool operator == (FrameKey const & rvalue) const = default;
};


struct FrameKeyHash
{
	std::size_t operator () (FrameKey const & key) const
	{
		std::size_t hash = std::hash<void const *>()(key.Shape);
		hash ^= (std::size_t)key.Index * 0x9E3779B9u + (hash << 6) + (hash >> 2);
		hash ^= (std::size_t)key.Factor * 0x85EBCA6Bu + (hash << 6) + (hash >> 2);
		return(hash);
	}
};


struct FrameEntry
{
	std::vector<unsigned char> Pixels;
	std::vector<unsigned char> Encoded;
	int Width = 0;
	int Height = 0;
};


// Most recently drawn at the front, so eviction takes from the back.
std::list<FrameKey> Order;
std::unordered_map<FrameKey, std::pair<FrameEntry, std::list<FrameKey>::iterator>, FrameKeyHash> Frames;
unsigned HeldBytes = 0;


// Two entries are kept whatever the budget says, because one draw asks for a shape and its
// depth shape before it blits either.
constexpr std::size_t MINIMUM_HELD = 2;


void Trim_To_Budget(void)
{
	while (HeldBytes > SHAPE_EXPAND_BUDGET && Order.size() > MINIMUM_HELD) {
		FrameKey const & oldest = Order.back();
		auto found = Frames.find(oldest);
		if (found != Frames.end()) {
			HeldBytes -= (unsigned)(found->second.first.Pixels.size() + found->second.first.Encoded.size());
			Frames.erase(found);
		}
		Order.pop_back();
	}
}

}


namespace
{

int Factor_For(ShapeSet const * shapefile, int scale)
{
	if (shapefile == NULL) {
		return(1);
	}

	int const shapescale = Shape_Scale(shapefile);
	int const factor = (shapescale > 0) ? scale / shapescale : 1;
	return((factor > 1) ? factor : 1);
}

}


int Shape_Draw_Factor(ShapeSet const * shapefile)
{
	return(Factor_For(shapefile, Draw_Scale()));
}


int Shape_World_Factor(ShapeSet const * shapefile)
{
	return(Factor_For(shapefile, Asset_Scale()));
}


namespace
{

FrameEntry * Entry_For(ShapeSet const * shapefile, int shapenum, int factor)
{
	if (shapefile == NULL || factor < 1) {
		return(NULL);
	}

	FrameKey const key{shapefile, shapenum, factor};

	auto found = Frames.find(key);
	if (found != Frames.end()) {
		Order.splice(Order.begin(), Order, found->second.second);
		found->second.second = Order.begin();
		return(&found->second.first);
	}

	Rect const rect = shapefile->Get_Rect(shapenum);
	void const * data = shapefile->Get_Data(shapenum);
	if (data == NULL || rect.Width <= 0 || rect.Height <= 0) {
		return(NULL);
	}

	unsigned char const * source = (unsigned char const *)data;
	std::vector<unsigned char> decoded;

	if (shapefile->Is_RLE_Compressed(shapenum)) {
		decoded.resize((std::size_t)rect.Width * rect.Height);
		// The shape format's per-frame size is not populated by anything here, so the row
		// framing is trusted exactly as the engine's RLE blitter trusts it.
		if (!Scale_Decode_RLE_Frame(data, 0, rect.Width, rect.Height, decoded.data())) {
			return(NULL);
		}
		source = decoded.data();
	}

	FrameEntry entry;
	entry.Width = rect.Width * factor;
	entry.Height = rect.Height * factor;
	entry.Pixels.resize((std::size_t)entry.Width * entry.Height);
	Scale_Expand_8Bit(source, rect.Width, rect.Height, entry.Pixels.data(), factor);

	HeldBytes += (unsigned)entry.Pixels.size();

	Order.push_front(key);
	auto inserted = Frames.emplace(key, std::make_pair(std::move(entry), Order.begin())).first;

	Trim_To_Budget();

	return(&inserted->second.first);
}

}


ExpandedFrame Shape_Expanded_Frame(ShapeSet const * shapefile, int shapenum, int factor)
{
	ExpandedFrame result;

	FrameEntry const * entry = Entry_For(shapefile, shapenum, factor);
	if (entry == NULL) {
		return(result);
	}

	result.Data = entry->Pixels.data();
	result.Width = entry->Width;
	result.Height = entry->Height;
	return(result);
}


ExpandedFrame Shape_Expanded_RLE_Frame(ShapeSet const * shapefile, int shapenum, int factor)
{
	ExpandedFrame result;

	FrameEntry * entry = Entry_For(shapefile, shapenum, factor);
	if (entry == NULL) {
		return(result);
	}

	if (entry->Encoded.empty()) {
		entry->Encoded.resize((std::size_t)Scale_Encoded_RLE_Bound(entry->Width, entry->Height));

		int const written = Scale_Encode_RLE_Frame(entry->Pixels.data(), entry->Width, entry->Height, entry->Encoded.data());
		if (written <= 0) {
			entry->Encoded.clear();
			return(result);
		}

		entry->Encoded.resize((std::size_t)written);
		HeldBytes += (unsigned)written;
	}

	result.Data = entry->Encoded.data();
	result.Width = entry->Width;
	result.Height = entry->Height;
	return(result);
}


void Shape_Expansion_Reset(void)
{
	Frames.clear();
	Order.clear();
	HeldBytes = 0;
}


unsigned Shape_Expansion_Bytes(void)
{
	return(HeldBytes);
}
