/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/overlay.h"

#include <cstring>

#include "common/debug.h"

namespace STG {

bool Overlay::load(Common::SeekableReadStream &stream) {
	_surface.free();
	_mask.clear();

	char magic[8];
	if (stream.read(magic, 8) != 8 || memcmp(magic, "OVERLAY\0", 8) != 0) {
		warning("Overlay: missing OVERLAY header");
		return false;
	}
	uint32 width = stream.readUint32LE();
	uint32 rows = stream.readUint32LE();
	if (width == 0 || width > 4096 || rows == 0 || rows > 65536) {
		warning("Overlay: implausible size %ux%u", width, rows);
		return false;
	}

	std::vector<uint32> offsets(rows);
	for (uint32 &offset : offsets)
		offset = stream.readUint32LE();
	int64 base = stream.pos();
	int64 dataSize = stream.size() - base;

	_surface.create((int16)width, (int16)rows, Graphics::PixelFormat::createFormatCLUT8());
	_mask.assign((size_t)width * rows, 0);

	for (uint32 row = 0; row < rows; row++) {
		if (offsets[row] > dataSize || !stream.seek(base + offsets[row])) {
			warning("Overlay: bad offset for row %u", row);
			return false;
		}
		byte *pixels = (byte *)_surface.getBasePtr(0, row);
		byte *mask = &_mask[(size_t)row * width];
		uint32 x = 0;
		while (x < width) {
			int16 n = stream.readSint16LE();
			if (stream.eos()) {
				warning("Overlay: row %u is truncated", row);
				return false;
			}
			if (n < 0) {
				x += -n;
				continue;
			}
			if (n == 0 || x + n > width) {
				warning("Overlay: row %u has a run of %d at x=%u (width %u)", row, n, x, width);
				return false;
			}
			stream.read(pixels + x, n);
			memset(mask + x, 1, n);
			x += n;
			if (n & 1)
				stream.readByte(); // pad to even
		}
		if (x != width) {
			warning("Overlay: row %u is %u pixels wide, expected %u", row, x, width);
			return false;
		}
	}
	return true;
}

void Overlay::drawFrame(Graphics::Surface &dst, int frame, int frameHeight, int x, int y) const {
	int top = frame * frameHeight;
	for (int fy = 0; fy < frameHeight && top + fy < _surface.h; fy++) {
		const byte *src = (const byte *)_surface.getBasePtr(0, top + fy);
		const byte *mask = &_mask[(size_t)(top + fy) * _surface.w];
		for (int fx = 0; fx < _surface.w; fx++) {
			if (mask[fx])
				dst.setPixel(x + fx, y + fy, src[fx]);
		}
	}
}

int Overlay::guessFrameHeight() const {
	const int rows = _surface.h, width = _surface.w;
	if (rows % 252 == 0)
		return 252;

	int best = rows;
	double bestScore = 1e9;
	for (int h = 48; h <= 320 && h <= rows / 2; h++) {
		if (rows % h)
			continue;
		// Fraction of pixels whose opacity changes from one frame to the next.
		long changed = 0;
		int frames = rows / h;
		for (int f = 0; f + 1 < frames; f++) {
			for (int y = 0; y < h; y++) {
				const byte *a = &_mask[(size_t)(f * h + y) * width];
				const byte *b = &_mask[(size_t)((f + 1) * h + y) * width];
				for (int x = 0; x < width; x++)
					changed += a[x] != b[x];
			}
		}
		double score = (double)changed / ((double)(frames - 1) * h * width);
		if (score < bestScore) {
			bestScore = score;
			best = h;
		}
	}
	return best;
}

} // End of namespace STG
