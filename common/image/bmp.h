/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_IMAGE_BMP_H
#define COMMON_IMAGE_BMP_H

#include "common/graphics/palette.h"
#include "common/graphics/surface.h"
#include "common/stream.h"

namespace Image {

/**
 * Decoder for Windows device independent bitmaps.
 *
 * 8-bit images (uncompressed or RLE8) decode to a CLUT8 surface plus a
 * palette; 16, 24 and 32-bit images decode to ARGB8888.
 */
class BitmapDecoder {
public:
	/**
	 * Decode a .BMP file, starting with the "BM" file header.
	 */
	bool loadStream(Common::SeekableReadStream &stream);

	/**
	 * Decode a bare DIB starting at its BITMAPINFOHEADER, as stored in
	 * Windows executable resources.
	 */
	bool loadDIB(Common::SeekableReadStream &stream);

	const Graphics::Surface &getSurface() const { return _surface; }
	const Graphics::Palette &getPalette() const { return _palette; }
	bool hasPalette() const { return _surface.format.isCLUT8(); }
	uint16 getPaletteColorCount() const { return _paletteColorCount; }

private:
	bool loadInternal(Common::SeekableReadStream &stream, int64 dataOffset);
	bool decodeRLE8(Common::SeekableReadStream &stream, int width, int height, bool bottomUp);

	Graphics::Surface _surface;
	Graphics::Palette _palette;
	uint16 _paletteColorCount = 0;
};

} // End of namespace Image

#endif
