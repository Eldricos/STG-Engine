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
#include <cmath>
#include <cstdlib>
#include <map>

#include "common/debug.h"
#include "common/endian.h"
#include "common/serializer.h"

namespace STG {

const uint32 LevelFile::kRecordSizes[kRecordSizeCount] = { 1024, 8, 292, 152, 140, 276, 196, 64, 24 };

namespace {

const uint32 kHeaderSize = 48;
const uint32 kEntrySize = 16;
const uint32 kTextureHeaderSize = 20;

LevelFile::LightInfo readLight(const byte *record) {
	LevelFile::LightInfo light;
	light.group = (int16)READ_LE_UINT16(record + 0x64);
	light.brightness = READ_LE_UINT16(record + 0x66);
	light.level = record[0x68];
	return light;
}

} // End of anonymous namespace

void LevelFile::updateWallGeometry(int index) {
	Wall &wall = _walls[index];
	double x1 = _vertices[wall.vertex1].x / 65536.0, y1 = _vertices[wall.vertex1].y / 65536.0;
	double x2 = _vertices[wall.vertex2].x / 65536.0, y2 = _vertices[wall.vertex2].y / 65536.0;
	double dx = x2 - x1, dy = y2 - y1, length = std::sqrt(dx * dx + dy * dy);
	if (length <= 0)
		return;
	double nx = dy / length, ny = -dx / length;
	wall.normalX = (int32)std::lround(nx * 65536);
	wall.normalY = (int32)std::lround(ny * 65536);
	wall.distance = (int32)std::lround(-(nx * x2 + ny * y2) * 65536);
}

void LevelFile::LightInfo::sync(Common::Serializer &s) {
	s.syncAsSint16(group);
	s.syncAsUint16(brightness);
	s.syncAsByte(level);
}

void LevelFile::syncState(Common::Serializer &s) {
	s.syncCount((uint32)_walls.size());
	for (Wall &wall : _walls) {
		if (s.err())
			return;
		s.syncAsSint16(wall.uScroll);
		s.syncAsSint16(wall.vScroll);
		s.syncAsUint16(wall.texture);
		s.syncAsUint16(wall.frames);
		s.syncAsUint16(wall.speed);
		s.syncAsUint32(wall.flags);
		wall.light.sync(s);
		s.syncAsBool(wall.fullbright);
	}
	s.syncCount((uint32)_sectors.size());
	for (Sector &sector : _sectors) {
		if (s.err())
			return;
		s.syncAsSint32(sector.height);
		s.syncAsUint64(sector.activeZones);
		s.syncAsSint16(sector.uScroll);
		s.syncAsSint16(sector.vScroll);
		s.syncVector(sector.zoneBands, [](Common::Serializer &ser, std::pair<int32, uint64> &band) {
			ser.syncAsSint32(band.first);
			ser.syncAsUint64(band.second);
		});
		s.syncAsUint16(sector.texture);
		s.syncAsUint16(sector.frames);
		s.syncAsUint16(sector.speed);
		s.syncAsUint32(sector.flags);
		sector.light.sync(s);
		s.syncAsBool(sector.fullbright);
	}
	s.syncCount((uint32)_billboards.size());
	for (Billboard &billboard : _billboards) {
		if (s.err())
			return;
		s.syncAsSint32(billboard.x);
		s.syncAsSint32(billboard.y);
		s.syncAsSint32(billboard.z);
		s.syncAsUint16(billboard.texture);
		s.syncAsUint16(billboard.frames);
		s.syncAsUint16(billboard.speed);
		s.syncAsUint32(billboard.flags);
		billboard.light.sync(s);
		s.syncAsUint64(billboard.zones);
		s.syncAsBool(billboard.hidden);
		s.syncAsBool(billboard.fullbright);
	}
}

int LevelFile::LightInfo::getLevel(int percent) const {
	// As the original computes it at 0x4BE0E0 (on load) and 0x4B7BA0 (when
	// a script changes a group's brightness).
	if (group == 0)
		return std::min<int>(level, kLightLevels - 1);
	uint32 scaled = (uint32)std::max(percent, 0) * brightness / (3200 * 100);
	return kLightLevels - 1 - (int)std::min<uint32>(scaled, kLightLevels - 1);
}

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
	_billboards.clear();
	_actors.clear();

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
	// Windows RGBQUADs: blue, green, red, unused. The console's bitmaps
	// share these indices, which is how their colours were matched up.
	for (uint32 i = 0; i < std::min<uint32>(count, Graphics::Palette::kSize); i++)
		_palette.set(i, p[i * 4 + 2], p[i * 4 + 1], p[i * 4]);

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
		wall.top = (int32)READ_LE_UINT32(r + 0x2C);
		wall.bottom = (int32)READ_LE_UINT32(r + 0x30);
		wall.uStart = (int32)READ_LE_UINT32(r + 0x44);
		wall.uLength = (int32)READ_LE_UINT32(r + 0x48);
		wall.uEnd1 = (int32)READ_LE_UINT32(r + 0x70);
		wall.uEnd2 = (int32)READ_LE_UINT32(r + 0x74);
		wall.vStart = (int32)READ_LE_UINT32(r + 0x4C);
		wall.vLength = (int32)READ_LE_UINT32(r + 0x50);
		wall.flags = READ_LE_UINT32(r + 0x6C);
		wall.speed = READ_LE_UINT16(r + 0x80);
		wall.frames = READ_LE_UINT16(r + 0x82);
		wall.objectId = READ_LE_UINT16(r + 0x14);
		wall.texture = READ_LE_UINT16(r + 0x96);
		wall.light = readLight(r);
		wall.zones = READ_LE_UINT64(r + 0x3C);
		wall.moverId = (int16)READ_LE_UINT16(r + 0x8A);
		wall.moverMode = (int16)READ_LE_UINT16(r + 0x8C);
		wall.moverSpeed = READ_LE_UINT16(r + 0x8E);
		wall.moverWait = READ_LE_UINT16(r + 0x90);
		wall.bumpMover = (int16)READ_LE_UINT16(r + 0x92);
		wall.bumpMode = (int16)READ_LE_UINT16(r + 0x94);
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
		sector.height = (int32)READ_LE_UINT32(r + 0x20);
		sector.moverId = (int16)READ_LE_UINT16(r + 0x106);
		sector.moverMode = (int16)READ_LE_UINT16(r + 0x108);
		sector.uOffset = (int32)READ_LE_UINT32(r + 0x44);
		sector.uScale = (int32)READ_LE_UINT32(r + 0x48);
		sector.vOffset = (int32)READ_LE_UINT32(r + 0x4C);
		sector.vScale = (int32)READ_LE_UINT32(r + 0x50);
		sector.light = readLight(r);
		sector.zones = READ_LE_UINT64(r + 0x3C);
		sector.activeZones = READ_LE_UINT64(r + 0x74);
		sector.frames = READ_LE_UINT16(r + 0x100);
		sector.speed = READ_LE_UINT16(r + 0x102);
		sector.objectId = READ_LE_UINT16(r + 0x14);
		sector.texture = READ_LE_UINT16(r + 0x11C);
		sector.rotation = READ_LE_UINT16(r + 0x11E);
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

	if (!table(kChunkBillboards, kRecordSizes[4], p, count))
		return false;
	for (uint32 i = 0; i < count; i++) {
		const byte *r = p + i * kRecordSizes[4];
		Billboard billboard;
		billboard.x = (int32)READ_LE_UINT32(r + 0x1C);
		billboard.z = (int32)READ_LE_UINT32(r + 0x20);
		billboard.y = (int32)READ_LE_UINT32(r + 0x24);
		billboard.halfWidth = (int32)READ_LE_UINT32(r + 0x34);
		billboard.height = (int32)READ_LE_UINT32(r + 0x38);
		billboard.flags = READ_LE_UINT32(r + 0x6C);
		billboard.speed = READ_LE_UINT16(r + 0x84);
		billboard.frames = READ_LE_UINT16(r + 0x86);
		billboard.radius = (int32)READ_LE_UINT32(r + 0x54);
		billboard.boxHeight = (int32)READ_LE_UINT32(r + 0x58);
		billboard.objectId = READ_LE_UINT16(r + 0x14);
		billboard.texture = READ_LE_UINT16(r + 0x8A);
		billboard.light = readLight(r);
		billboard.zones = READ_LE_UINT64(r + 0x3C);
		_billboards.push_back(billboard);
	}

	if (!table(kChunkActors, kRecordSizes[5], p, count))
		return false;
	for (uint32 i = 0; i < count; i++) {
		const byte *r = p + i * kRecordSizes[5];
		Actor actor;
		actor.objectId = READ_LE_UINT16(r + 0x14);
		actor.x = (int32)READ_LE_UINT32(r + 0x1C);
		actor.z = (int32)READ_LE_UINT32(r + 0x20);
		actor.y = (int32)READ_LE_UINT32(r + 0x24);
		actor.reach = (int32)READ_LE_UINT32(r + 0x28);
		actor.drawOffset = (int32)READ_LE_UINT32(r + 0x30);
		actor.scale = (int32)READ_LE_UINT32(r + 0x34);
		actor.height = (int32)READ_LE_UINT32(r + 0x38);
		actor.radius = (int32)READ_LE_UINT32(r + 0x54);
		actor.bodyHeight = (int32)READ_LE_UINT32(r + 0x58);
		actor.light = readLight(r);
		actor.flags = READ_LE_UINT32(r + 0x6C);
		actor.outerRadius = (int32)READ_LE_UINT32(r + 0x90);
		actor.innerRadius = (int32)READ_LE_UINT32(r + 0x94);
		actor.homeRadius = (int32)READ_LE_UINT32(r + 0x98);
		actor.minWalk = (int32)READ_LE_UINT32(r + 0x9C);
		actor.maxWalk = (int32)READ_LE_UINT32(r + 0xA0);
		actor.stepHeight = (int32)READ_LE_UINT32(r + 0xAC);
		actor.muzzleX = (int32)READ_LE_UINT32(r + 0x84);
		actor.muzzleZ = (int32)READ_LE_UINT32(r + 0x88);
		actor.muzzleY = (int32)READ_LE_UINT32(r + 0x8C);
		for (int m = 0; m < kAnimationCount; m++) {
			const byte *a = r + 0xC4 + m * 8;
			Animation &anim = actor.animations[m];
			// With the original loader's fixes (0x4BE39B..0x4BE3DB).
			anim.speed = READ_LE_UINT16(a) ? READ_LE_UINT16(a) : 0x40;
			anim.rate = a[2];
			anim.frames = a[3] ? a[3] : 1;
			anim.loopStart = std::min<byte>(a[4], (byte)(anim.frames - 1));
			anim.directions = a[5];
			anim.mirror = a[6];
		}
		actor.spriteSet = READ_LE_UINT16(r + 0xFC);
		actor.minTurn = READ_LE_UINT16(r + 0x100);
		actor.maxTurn = READ_LE_UINT16(r + 0x102);
		actor.turnSpeed = READ_LE_UINT16(r + 0x104);
		actor.heading = READ_LE_UINT16(r + 0x108);
		actor.guardMode = r[0x10D];
		actor.mode = r[0x10E];
		_actors.push_back(actor);
	}
	return true;
}

uint32 LevelFile::guessSkyHeight(const byte *pixels, uint32 size) const {
	// The header's height doesn't always fit (halee1: 252 for a 512 x 128
	// sky), so take the width at which neighbouring rows match best. Twice
	// the true width matches nearly as well, two rows side by side, so a
	// width whose half does about as well gives way to the half.
	std::map<uint32, double> costs;
	for (uint32 width = 64; width <= 4096; width++) {
		if (size % width != 0 || size / width < 16)
			continue;
		uint32 height = size / width;
		double cost = 0;
		int samples = 0;
		for (uint32 y = 0; y + 1 < height; y += std::max<uint32>(1, height / 32)) {
			for (uint32 x = 0; x < width; x += std::max<uint32>(1, width / 64)) {
				byte r1, g1, b1, r2, g2, b2;
				_palette.get(pixels[y * width + x], r1, g1, b1);
				_palette.get(pixels[(y + 1) * width + x], r2, g2, b2);
				cost += std::abs(r1 - r2) + std::abs(g1 - g2) + std::abs(b1 - b2);
				samples++;
			}
		}
		costs[width] = cost / samples;
	}
	uint32 best = 0;
	for (const auto &c : costs) {
		if (!best || c.second < costs[best])
			best = c.first;
	}
	if (!best)
		return 0;
	for (auto half = costs.find(best / 2); best % 2 == 0 && half != costs.end() && half->second <= costs[best] * 1.1;
	     half = costs.find(best / 2))
		best /= 2;
	return size / best;
}

bool LevelFile::loadSky(Graphics::Surface &sky) const {
	const uint32 headerSize = 64;
	const ChunkEntry *entry = nullptr;
	for (const ChunkEntry &e : _directory) {
		if (e.type == kChunkSky) {
			entry = &e;
			break;
		}
	}
	if (!entry || entry->size < headerSize)
		return false;

	const byte *p = &_data[entry->offset];
	uint32 size = READ_LE_UINT32(p + 16);
	uint32 height = READ_LE_UINT32(p + 24);
	if (size == 0 || headerSize + (uint64)size > entry->size) {
		warning("LevelFile: sky has a bad size");
		return false;
	}
	if (height == 0 || size % height != 0 || size / height > 4096)
		height = guessSkyHeight(p + headerSize, size);
	if (height == 0) {
		warning("LevelFile: sky has a bad size");
		return false;
	}
	uint32 width = size / height;
	sky.create((int16)width, (int16)height, Graphics::PixelFormat::createFormatCLUT8());
	sky.copyRectToSurface(p + headerSize, (int)width, 0, 0, (int)width, (int)height);
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
	// pixels are column-major, each column bottom first, followed by 3
	// bytes of hints per column.
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
			texture.surface.setPixel(x, height - 1 - y, pixels[x * height + y]);
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
