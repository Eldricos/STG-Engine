/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/level.h"

#include <algorithm>
#include <climits>

#include "common/debug.h"
#include "common/endian.h"

namespace STG {

const uint32 LevelFile::kRecordSizes[kRecordSizeCount] = { 1024, 8, 292, 152, 140, 276, 196, 64, 24 };

namespace {

const uint32 kHeaderSize = 48;
const uint32 kEntrySize = 16;
const uint32 kTextureHeaderSize = 20;

} // End of anonymous namespace

bool LevelFile::load(Common::SeekableReadStream &stream) {
	_directory.clear();
	_data = stream.readAll();
	if (_data.size() < kHeaderSize) {
		warning("LevelFile: file too short");
		return false;
	}

	if (READ_LE_UINT32(&_data[0]) != kVersion) {
		warning("LevelFile: unsupported version %u", READ_LE_UINT32(&_data[0]));
		return false;
	}
	for (int i = 0; i < kRecordSizeCount; i++) {
		uint32 size = READ_LE_UINT32(&_data[4 + i * 4]);
		if (size != kRecordSizes[i]) {
			warning("LevelFile: record size %d is %u, expected %u", i, size, kRecordSizes[i]);
			return false;
		}
	}

	uint32 count = READ_LE_UINT32(&_data[40]);
	if ((uint64)kHeaderSize + (uint64)count * kEntrySize > _data.size()) {
		warning("LevelFile: directory of %u entries does not fit", count);
		return false;
	}
	for (uint32 i = 0; i < count; i++) {
		const byte *p = &_data[kHeaderSize + i * kEntrySize];
		ChunkEntry entry = { READ_LE_UINT32(p), READ_LE_UINT32(p + 4), READ_LE_UINT16(p + 8),
		                     READ_LE_UINT16(p + 10), READ_LE_UINT16(p + 12), READ_LE_UINT16(p + 14) };
		if ((uint64)entry.offset + entry.size > _data.size()) {
			warning("LevelFile: chunk %u (type %u) lies outside the file", i, entry.type);
			return false;
		}
		_directory.push_back(entry);
	}
	return loadTables();
}

const LevelFile::ChunkEntry *LevelFile::findChunk(uint16 type, uint16 id) const {
	for (const ChunkEntry &entry : _directory) {
		if (entry.type == type && entry.id == id)
			return &entry;
	}
	return nullptr;
}

std::vector<byte> LevelFile::getChunkData(uint16 type, uint16 id) const {
	const ChunkEntry *entry = findChunk(type, id);
	if (!entry)
		return {};
	return std::vector<byte>(_data.begin() + entry->offset, _data.begin() + entry->offset + entry->size);
}

bool LevelFile::loadTables() {
	_vertices.clear();
	_walls.clear();
	_sectors.clear();

	// Each table's record count is in count B; check it against the size.
	auto table = [this](uint16 type, uint32 recordSize, const byte *&data, uint32 &count) {
		const ChunkEntry *entry = findChunk(type, 1);
		data = nullptr;
		count = 0;
		if (!entry)
			return true;
		count = entry->countB;
		if ((uint64)count * recordSize > entry->size) {
			warning("LevelFile: table %u has %u records of %u bytes but only %u bytes", type, count, recordSize,
			        entry->size);
			return false;
		}
		data = &_data[entry->offset];
		return true;
	};

	const byte *p;
	uint32 count;

	if (!table(kChunkPalette, 4, p, count))
		return false;
	for (uint32 i = 0; i < std::min<uint32>(count, Graphics::Palette::kSize); i++)
		_palette.set(i, p[i * 4], p[i * 4 + 1], p[i * 4 + 2]);

	if (!table(kChunkVertices, kRecordSizes[1], p, count))
		return false;
	for (uint32 i = 0; i < count; i++)
		_vertices.push_back({ (int32)READ_LE_UINT32(p + i * 8), (int32)READ_LE_UINT32(p + i * 8 + 4) });

	if (!table(kChunkWalls, kRecordSizes[3], p, count))
		return false;
	for (uint32 i = 0; i < count; i++) {
		const byte *r = p + i * kRecordSizes[3];
		Wall wall;
		wall.vertex1 = READ_LE_UINT16(r + 134);
		wall.vertex2 = READ_LE_UINT16(r + 136);
		wall.normalX = (int32)READ_LE_UINT32(r + 28);
		wall.normalY = (int32)READ_LE_UINT32(r + 36);
		wall.distance = (int32)READ_LE_UINT32(r + 40);
		if (wall.vertex1 >= _vertices.size() || wall.vertex2 >= _vertices.size()) {
			warning("LevelFile: wall %u refers to a missing vertex", i);
			return false;
		}
		_walls.push_back(wall);
	}

	if (!table(kChunkSectors, kRecordSizes[2], p, count))
		return false;
	for (uint32 i = 0; i < count; i++) {
		const byte *r = p + i * kRecordSizes[2];
		Sector sector;
		sector.flags = READ_LE_UINT32(r + 108);
		uint16 n = READ_LE_UINT16(r + 266);
		if (n > 8) {
			warning("LevelFile: sector %u has %u vertices", i, n);
			return false;
		}
		for (uint16 k = 0; k < n; k++) {
			uint16 vertex = READ_LE_UINT16(r + 268 + k * 2);
			if (vertex >= _vertices.size()) {
				warning("LevelFile: sector %u refers to a missing vertex", i);
				return false;
			}
			sector.vertices.push_back(vertex);
		}
		sector.raw.assign(r, r + kRecordSizes[2]);
		_sectors.push_back(std::move(sector));
	}
	return true;
}

std::vector<uint16> LevelFile::getTextureIds() const {
	std::vector<uint16> ids;
	for (const ChunkEntry &entry : _directory) {
		if (entry.type == kChunkTexture && entry.size > kTextureHeaderSize)
			ids.push_back(entry.id);
	}
	return ids;
}

bool LevelFile::loadTexture(uint16 id, Texture &texture) const {
	const ChunkEntry *entry = findChunk(kChunkTexture, id);
	if (!entry || entry->size < kTextureHeaderSize)
		return false;

	// 12 bytes reserved for pointers, then id, width, height and flags;
	// pixels are column-major, followed by 3 bytes of hints per column.
	const byte *p = &_data[entry->offset];
	uint16 width = READ_LE_UINT16(p + 14);
	uint16 height = READ_LE_UINT16(p + 16);
	if (width == 0 || height == 0 || kTextureHeaderSize + (uint64)width * height > entry->size) {
		warning("LevelFile: texture %u has a bad size %ux%u", id, width, height);
		return false;
	}

	texture.id = READ_LE_UINT16(p + 12);
	texture.flags = READ_LE_UINT16(p + 18);
	texture.surface.create((int16)width, (int16)height, Graphics::PixelFormat::createFormatCLUT8());
	const byte *pixels = p + kTextureHeaderSize;
	for (int x = 0; x < width; x++) {
		for (int y = 0; y < height; y++)
			texture.surface.setPixel(x, y, pixels[x * height + y]);
	}
	return true;
}

void drawLevelMap(const LevelFile &level, Graphics::Surface &dst) {
	const std::vector<LevelFile::Vertex> &vertices = level.getVertices();
	if (vertices.empty())
		return;

	int64 minX = INT32_MAX, minY = INT32_MAX, maxX = INT32_MIN, maxY = INT32_MIN;
	for (const LevelFile::Vertex &v : vertices) {
		minX = std::min<int64>(minX, v.x);
		maxX = std::max<int64>(maxX, v.x);
		minY = std::min<int64>(minY, v.y);
		maxY = std::max<int64>(maxY, v.y);
	}
	const int margin = 4;
	double spanX = (double)std::max<int64>(1, maxX - minX), spanY = (double)std::max<int64>(1, maxY - minY);
	// Screen pixels per map unit, preserving aspect. Maps span tens of
	// millions of 16.16 units, so integer maths would round this to 0.
	double scale = std::min((dst.w - 2 * margin) / spanX, (dst.h - 2 * margin) / spanY);
	double offsetX = (dst.w - spanX * scale) / 2;
	double offsetY = (dst.h - spanY * scale) / 2;
	auto project = [&](const LevelFile::Vertex &v) {
		// Map y grows upwards; screen y grows downwards.
		return Common::Point((int16)(offsetX + (v.x - minX) * scale + 0.5),
		                     (int16)(offsetY + (maxY - v.y) * scale + 0.5));
	};

	const Graphics::Palette &palette = level.getPalette();
	byte outline = palette.findBestColor(0, 0, 0);
	byte wallColor = palette.findBestColor(120, 220, 255);

	// Sectors in varied colours so neighbours tend to differ.
	const std::vector<LevelFile::Sector> &sectors = level.getSectors();
	for (size_t i = 0; i < sectors.size(); i++) {
		std::vector<Common::Point> points;
		for (uint16 index : sectors[i].vertices)
			points.push_back(project(vertices[index]));
		byte r = (byte)(40 + (i * 53) % 100), g = (byte)(60 + (i * 97) % 120), b = (byte)(120 + (i * 31) % 120);
		dst.fillPolygon(points.data(), (int)points.size(), palette.findBestColor(r, g, b));
		for (size_t k = 0; k < points.size(); k++) {
			const Common::Point &a = points[k], &b2 = points[(k + 1) % points.size()];
			dst.drawLine(a.x, a.y, b2.x, b2.y, outline);
		}
	}

	for (const LevelFile::Wall &wall : level.getWalls()) {
		Common::Point a = project(vertices[wall.vertex1]), b = project(vertices[wall.vertex2]);
		dst.drawLine(a.x, a.y, b.x, b.y, wallColor);
	}
}

} // End of namespace STG
