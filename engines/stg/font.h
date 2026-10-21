/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_FONT_H
#define STG_FONT_H

#include <map>
#include <vector>

#include "common/graphics/surface.h"
#include "common/stream.h"
#include "common/str.h"

namespace STG {

/**
 * A bitmap font in the game's .FNT format (e.g. COMMON.DAT:FONTS/ACTION.FNT).
 *
 * Layout, worked out from ACTION.FNT and CREDITS.FNT:
 *
 *   char[4]  "FONT"
 *   uint32   size of the rest of the file
 *   uint16   number of glyphs
 *   uint8    transparent colour index
 *   uint8    line height
 *   uint32   unknown, 0 in both fonts seen
 *   glyphs, each:
 *     uint16 character code
 *     uint16 size of the rest of the glyph (= width * height + 2)
 *     uint8  width
 *     uint8  height (glyphs are top aligned; may be less than the line height)
 *     uint8  pixels[height][width], palette indices
 */
class Font {
public:
	bool load(Common::SeekableReadStream &stream);

	bool isLoaded() const { return !_glyphs.empty(); }
	int getHeight() const { return _height; }
	byte getTransparentColor() const { return _transparent; }
	int getGlyphCount() const { return (int)_glyphs.size(); }
	bool hasGlyph(uint16 c) const { return _glyphs.count(c) != 0; }

	int getCharWidth(uint16 c) const;
	int getStringWidth(const Common::String &str) const;

	/**
	 * Draw text with its top left corner at (x, y) in the glyphs' own
	 * colours. The glyphs have dark outline pixels, so drawing them all in
	 * one colour makes text unreadable; instead, remap (if given) translates
	 * each glyph colour index into the destination palette.
	 */
	void drawChar(Graphics::Surface &dst, uint16 c, int x, int y, const byte *remap = nullptr) const;
	void drawString(Graphics::Surface &dst, const Common::String &str, int x, int y, const byte *remap = nullptr) const;

	/** Split text into lines no wider than maxWidth, breaking at spaces. */
	Common::StringArray wordWrap(const Common::String &str, int maxWidth) const;

private:
	struct Glyph {
		int width = 0;
		int height = 0;
		std::vector<byte> pixels;
	};

	const Glyph *findGlyph(uint16 c) const;

	std::map<uint16, Glyph> _glyphs;
	byte _transparent = 0;
	int _height = 0;
};

} // End of namespace STG

#endif
