/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/font.h"

#include <cctype>

#include "common/debug.h"

namespace STG {

bool Font::load(Common::SeekableReadStream &stream) {
	_glyphs.clear();

	if (stream.readUint32BE() != MKTAG('F', 'O', 'N', 'T')) {
		warning("Font: missing FONT header");
		return false;
	}
	uint32 dataSize = stream.readUint32LE();
	uint16 count = stream.readUint16LE();
	_transparent = stream.readByte();
	_height = stream.readByte();
	stream.readUint32LE(); // unknown

	int64 end = std::min<int64>(8 + (int64)dataSize, stream.size());
	while (stream.pos() + 6 <= end) {
		uint16 code = stream.readUint16LE();
		uint16 size = stream.readUint16LE();
		Glyph glyph;
		glyph.width = stream.readByte();
		glyph.height = stream.readByte();
		if (size != glyph.width * glyph.height + 2) {
			warning("Font: glyph 0x%02x has size %u, expected %d", code, size, glyph.width * glyph.height + 2);
			return false;
		}
		glyph.pixels.resize(glyph.width * glyph.height);
		if (stream.read(glyph.pixels.data(), (uint32)glyph.pixels.size()) != glyph.pixels.size()) {
			warning("Font: truncated glyph 0x%02x", code);
			return false;
		}
		_glyphs[code] = std::move(glyph);
	}

	if (_glyphs.size() != count)
		warning("Font: header says %u glyphs, found %d", count, (int)_glyphs.size());
	debugC(1, "resources", "Font: %d glyphs, height %d, transparent %d", (int)_glyphs.size(), _height, _transparent);
	return !_glyphs.empty();
}

const Font::Glyph *Font::findGlyph(uint16 c) const {
	auto it = _glyphs.find(c);
	if (it == _glyphs.end() && c >= 'a' && c <= 'z')
		it = _glyphs.find((uint16)toupper(c)); // some fonts may be upper case only
	return it == _glyphs.end() ? nullptr : &it->second;
}

int Font::getCharWidth(uint16 c) const {
	const Glyph *glyph = findGlyph(c);
	return glyph ? glyph->width : 0;
}

int Font::getStringWidth(const Common::String &str) const {
	int width = 0;
	for (char c : str)
		width += getCharWidth((byte)c);
	return width;
}

void Font::drawChar(Graphics::Surface &dst, uint16 c, int x, int y, const byte *remap) const {
	const Glyph *glyph = findGlyph(c);
	if (!glyph)
		return;
	for (int gy = 0; gy < glyph->height; gy++) {
		for (int gx = 0; gx < glyph->width; gx++) {
			byte pixel = glyph->pixels[gy * glyph->width + gx];
			if (pixel != _transparent)
				dst.setPixel(x + gx, y + gy, remap ? remap[pixel] : pixel);
		}
	}
}

void Font::drawString(Graphics::Surface &dst, const Common::String &str, int x, int y, const byte *remap) const {
	for (char c : str) {
		drawChar(dst, (byte)c, x, y, remap);
		x += getCharWidth((byte)c);
	}
}

Common::StringArray Font::wordWrap(const Common::String &str, int maxWidth) const {
	Common::StringArray lines;
	Common::String line;
	for (const Common::String &word : Common::split(str, ' ')) {
		if (word.empty())
			continue;
		Common::String candidate = line.empty() ? word : line + " " + word;
		if (!line.empty() && getStringWidth(candidate) > maxWidth) {
			lines.push_back(line);
			line = word;
		} else {
			line = candidate;
		}
	}
	if (!line.empty())
		lines.push_back(line);
	return lines;
}

} // End of namespace STG
