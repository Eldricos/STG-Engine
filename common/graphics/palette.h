/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_GRAPHICS_PALETTE_H
#define COMMON_GRAPHICS_PALETTE_H

#include <cstring>

#include "common/types.h"

namespace Graphics {

/** A 256-colour palette stored as packed 8-bit R, G, B triplets. */
class Palette {
public:
	static const int kSize = 256;

	Palette() { memset(_data, 0, sizeof(_data)); }

	const byte *data() const { return _data; }
	byte *data() { return _data; }

	void set(int index, byte r, byte g, byte b) {
		_data[index * 3 + 0] = r;
		_data[index * 3 + 1] = g;
		_data[index * 3 + 2] = b;
	}

	void get(int index, byte &r, byte &g, byte &b) const {
		r = _data[index * 3 + 0];
		g = _data[index * 3 + 1];
		b = _data[index * 3 + 2];
	}

	/** Copy count RGB triplets into the palette starting at start. */
	void set(const byte *colors, int start, int count) {
		memcpy(_data + start * 3, colors, count * 3);
	}

	/**
	 * Convert from 6-bit-per-channel VGA DAC values, which some data files
	 * of the period still store.
	 */
	void setFromVGA(const byte *colors, int start, int count) {
		for (int i = 0; i < count * 3; i++) {
			byte v = colors[i] & 0x3F;
			_data[start * 3 + i] = (byte)((v << 2) | (v >> 4));
		}
	}

	/** Return the index of the closest matching colour. */
	byte findBestColor(byte r, byte g, byte b) const;

private:
	byte _data[kSize * 3];
};

} // End of namespace Graphics

#endif
