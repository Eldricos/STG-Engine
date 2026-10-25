/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/dissolve.h"

#include <algorithm>
#include <vector>

namespace STG {
namespace Dissolve {

namespace {

const int kHalf = 0x200;

void copyRow(byte *row, const byte *source, int width, int count, Common::RandomSource &random) {
	for (int i = 0; i < count; i++) {
		int x = (int)random.getRandomNumber(width - 1);
		row[x] = source[x];
	}
}

} // End of anonymous namespace

void speckle(Graphics::Surface &surface, byte colour, int amount, int style, Common::RandomSource &random) {
	int width = surface.w, height = surface.h;
	if (width <= 0 || height <= 0 || amount <= 0)
		return;
	amount = std::min(amount, 0x3FF);
	bool fill = amount >= kHalf;
	if (fill)
		amount = 0x3FF - amount;

	// Past half, the colour first and the picture over it.
	std::vector<byte> picture;
	if (fill) {
		picture.resize((size_t)width * height);
		for (int y = 0; y < height; y++)
			std::copy_n((const byte *)surface.getBasePtr(0, y), width, &picture[(size_t)y * width]);
		surface.fillRect(Common::Rect(width, height), colour);
	}

	if (style == 2) {
		int run = std::max(1, width / 16);
		int streaks = height * amount / 64;
		for (int i = 0; i < streaks; i++) {
			int y = (int)random.getRandomNumber(height - 1);
			int x = (int)random.getRandomNumber(width - 1);
			int n = std::min(run, width - x);
			byte *row = (byte *)surface.getBasePtr(x, y);
			if (fill)
				std::copy_n(&picture[(size_t)y * width + x], n, row);
			else
				std::fill_n(row, n, colour);
		}
		return;
	}

	int count = width * amount / 1024;
	for (int y = 0; y < height; y++) {
		byte *row = (byte *)surface.getBasePtr(0, y);
		if (fill) {
			copyRow(row, &picture[(size_t)y * width], width, count, random);
		} else {
			for (int i = 0; i < count; i++)
				row[random.getRandomNumber(width - 1)] = colour;
		}
	}
}

void crossfade(Graphics::Surface &surface, const Graphics::Surface &from, const Graphics::Surface &to, int amount,
               Common::RandomSource &random) {
	int width = std::min<int>({surface.w, from.w, to.w}), height = std::min<int>({surface.h, from.h, to.h});
	if (width <= 0 || height <= 0)
		return;
	amount = std::max(0, std::min(amount, 0x3FF));
	const Graphics::Surface *base = &from, *over = &to;
	if (amount >= kHalf) {
		std::swap(base, over);
		amount = 0x3FF - amount;
	}
	int count = width * amount / 1024;
	for (int y = 0; y < height; y++) {
		byte *row = (byte *)surface.getBasePtr(0, y);
		if (row != base->getBasePtr(0, y))
			std::copy_n((const byte *)base->getBasePtr(0, y), width, row);
		copyRow(row, (const byte *)over->getBasePtr(0, y), width, count, random);
	}
}

} // End of namespace Dissolve
} // End of namespace STG
