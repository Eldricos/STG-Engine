/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "common/graphics/palette.h"

namespace Graphics {

byte Palette::findBestColor(byte r, byte g, byte b) const {
	int best = 0;
	int bestDist = 0x7FFFFFFF;
	for (int i = 0; i < kSize; i++) {
		int dr = _data[i * 3 + 0] - r;
		int dg = _data[i * 3 + 1] - g;
		int db = _data[i * 3 + 2] - b;
		// Weighted towards green, roughly matching perceived brightness.
		int dist = dr * dr * 3 + dg * dg * 4 + db * db * 2;
		if (dist < bestDist) {
			bestDist = dist;
			best = i;
			if (dist == 0)
				break;
		}
	}
	return (byte)best;
}

} // End of namespace Graphics
