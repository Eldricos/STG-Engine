/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/sprite.h"

#include <algorithm>

#include "common/debug.h"

namespace STG {

bool SpriteSet::load(Common::SeekableReadStream &stream) {
	_frames.clear();
	_sequences.clear();

	uint32 version = stream.readUint32LE();
	uint32 frameCount = stream.readUint32LE();
	stream.readUint32LE();
	uint32 sequenceCount = stream.readUint32LE();
	stream.readUint32LE();
	stream.readUint32LE(); // size hint
	if (version != 2) {
		warning("SpriteSet: version %u is not supported", version);
		return false;
	}
	if (frameCount > 4096 || sequenceCount > 256) {
		warning("SpriteSet: implausible header (%u frames, %u sequences)", frameCount, sequenceCount);
		return false;
	}

	for (uint32 i = 0; i < sequenceCount; i++) {
		std::vector<byte> sequence(stream.readByte());
		stream.read(sequence.data(), (uint32)sequence.size());
		for (byte index : sequence) {
			if (index >= frameCount) {
				warning("SpriteSet: sequence %u refers to frame %u of %u", i, index, frameCount);
				return false;
			}
		}
		_sequences.push_back(std::move(sequence));
	}

	std::vector<byte> columns;
	for (uint32 i = 0; i < frameCount; i++) {
		uint32 width = stream.readUint32LE();
		uint32 columnCount = stream.readUint32LE();
		uint32 height = stream.readUint32LE();
		if (stream.eos() || columnCount == 0 || columnCount > 1024 || height == 0 || height > 1024) {
			warning("SpriteSet: bad header for frame %u", i);
			return false;
		}

		columns.resize(columnCount * height);
		if (stream.read(columns.data(), (uint32)columns.size()) != columns.size()) {
			warning("SpriteSet: frame %u is truncated", i);
			return false;
		}
		// Per-column tables: first and last solid rows, then a class: 0
		// empty, 1 with holes, 2 one run, 3 solid. The original skips empty
		// columns whatever their texels hold, and blank frames such as
		// amargos1/007.gs2's frame 0 rely on it: 2 columns of colour 0.
		std::vector<byte> tables(3 * columnCount);
		if (stream.read(tables.data(), (uint32)tables.size()) != tables.size()) {
			warning("SpriteSet: frame %u is truncated", i);
			return false;
		}

		Frame frame;
		frame.halfWidth = width / 65536.0;
		frame.surface.create((int16)columnCount, (int16)height, Graphics::PixelFormat::createFormatCLUT8());
		for (uint32 x = 0; x < columnCount; x++) {
			const byte *column = &columns[x * height];
			bool empty = tables[2 * columnCount + x] == 0;
			for (uint32 y = 0; y < height; y++)
				frame.surface.setPixel(x, height - 1 - y, empty ? kTransparent : column[y]);
		}
		_frames.push_back(std::move(frame));
	}
	return true;
}

int SpriteSet::getMaxWidth() const {
	int width = 0;
	for (const Frame &frame : _frames)
		width = std::max<int>(width, frame.surface.w);
	return width;
}

int SpriteSet::getMaxHeight() const {
	int height = 0;
	for (const Frame &frame : _frames)
		height = std::max<int>(height, frame.surface.h);
	return height;
}

} // End of namespace STG
