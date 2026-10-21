/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "test/test.h"

#include <cmath>
#include <cstring>

#include "common/endian.h"
#include "common/stream.h"
#include "engines/stg/actors.h"
#include "engines/stg/level.h"
#include "engines/stg/mission.h"
#include "engines/stg/opcodes.h"
#include "engines/stg/render3d.h"
#include "engines/stg/walker.h"

namespace {

struct Chunk {
	uint16 type;
	uint16 id;
	uint16 count;
	std::vector<byte> data;
};

/** Build an LV2 file from chunks, laying them out after the directory. */
std::vector<byte> makeLevelFile(const std::vector<Chunk> &chunks) {
	const uint32 headerSize = 48, entrySize = 16;
	std::vector<byte> file(headerSize + chunks.size() * entrySize);
	WRITE_LE_UINT32(&file[0], STG::LevelFile::kVersion);
	for (int i = 0; i < STG::LevelFile::kRecordSizeCount; i++)
		WRITE_LE_UINT32(&file[4 + i * 4], STG::LevelFile::kRecordSizes[i]);
	WRITE_LE_UINT32(&file[40], (uint32)chunks.size());
	WRITE_LE_UINT32(&file[44], (uint32)chunks.size() * entrySize);
	for (size_t i = 0; i < chunks.size(); i++) {
		byte *entry = &file[headerSize + i * entrySize];
		WRITE_LE_UINT32(entry, (uint32)chunks[i].data.size());
		WRITE_LE_UINT32(entry + 4, (uint32)file.size());
		WRITE_LE_UINT16(entry + 8, chunks[i].type);
		WRITE_LE_UINT16(entry + 10, chunks[i].id);
		WRITE_LE_UINT16(entry + 12, chunks[i].count);
		WRITE_LE_UINT16(entry + 14, chunks[i].count);
		file.insert(file.end(), chunks[i].data.begin(), chunks[i].data.end());
	}
	return file;
}

std::vector<Chunk> makeSquareLevel() {
	std::vector<Chunk> chunks;

	Chunk palette = { STG::LevelFile::kChunkPalette, 1, 256, std::vector<byte>(1024) };
	palette.data[4 * 7] = 200;
	chunks.push_back(palette);

	// A 2x2 unit square, in 16.16.
	const int32 corners[4][2] = { { 0, 0 }, { 2, 0 }, { 2, 2 }, { 0, 2 } };
	Chunk vertices = { STG::LevelFile::kChunkVertices, 1, 4, std::vector<byte>(32) };
	for (int i = 0; i < 4; i++) {
		WRITE_LE_UINT32(&vertices.data[i * 8], (uint32)(corners[i][0] << 16));
		WRITE_LE_UINT32(&vertices.data[i * 8 + 4], (uint32)(corners[i][1] << 16));
	}
	chunks.push_back(vertices);

	Chunk sectors = { STG::LevelFile::kChunkSectors, 1, 1, std::vector<byte>(292) };
	WRITE_LE_UINT32(&sectors.data[108], 0x12345678);
	WRITE_LE_UINT16(&sectors.data[266], 4);
	for (int i = 0; i < 4; i++)
		WRITE_LE_UINT16(&sectors.data[268 + i * 2], (uint16)i);
	chunks.push_back(sectors);

	Chunk walls = { STG::LevelFile::kChunkWalls, 1, 1, std::vector<byte>(152) };
	WRITE_LE_UINT32(&walls.data[28], 0x10000);
	WRITE_LE_UINT32(&walls.data[36], 0);
	WRITE_LE_UINT32(&walls.data[40], (uint32)-0x20000);
	WRITE_LE_UINT16(&walls.data[134], 1);
	WRITE_LE_UINT16(&walls.data[136], 2);
	chunks.push_back(walls);

	// A 2x3 texture, stored column by column.
	Chunk texture = { STG::LevelFile::kChunkTexture, 9, 0, std::vector<byte>(20 + 6 + 6) };
	WRITE_LE_UINT16(&texture.data[12], 9);
	WRITE_LE_UINT16(&texture.data[14], 2);
	WRITE_LE_UINT16(&texture.data[16], 3);
	WRITE_LE_UINT16(&texture.data[18], 0x40);
	for (int i = 0; i < 6; i++)
		texture.data[20 + i] = (byte)(10 + i);
	chunks.push_back(texture);
	return chunks;
}

bool loadLevel(STG::LevelFile &level, const std::vector<byte> &bytes) {
	Common::MemoryReadStream stream(bytes.data(), (uint32)bytes.size());
	return level.load(stream);
}

} // End of anonymous namespace

TEST(level_tables) {
	STG::LevelFile level;
	CHECK(loadLevel(level, makeLevelFile(makeSquareLevel())));

	CHECK_EQ(level.getDirectory().size(), (size_t)5);
	byte r, g, b;
	level.getPalette().get(7, r, g, b);
	CHECK_EQ(r, 200);

	CHECK_EQ(level.getVertices().size(), (size_t)4);
	CHECK_EQ(level.getVertices()[2].x, 2 << 16);
	CHECK_EQ(level.getVertices()[3].y, 2 << 16);

	CHECK_EQ(level.getSectors().size(), (size_t)1);
	const STG::LevelFile::Sector &sector = level.getSectors()[0];
	CHECK_EQ(sector.flags, 0x12345678u);
	CHECK_EQ(sector.vertices.size(), (size_t)4);
	CHECK_EQ(sector.vertices[3], 3);
	CHECK_EQ(sector.raw.size(), (size_t)292);

	CHECK_EQ(level.getWalls().size(), (size_t)1);
	const STG::LevelFile::Wall &wall = level.getWalls()[0];
	CHECK_EQ(wall.vertex1, 1);
	CHECK_EQ(wall.vertex2, 2);
	CHECK_EQ(wall.normalX, 0x10000);
	CHECK_EQ(wall.distance, -0x20000);

	CHECK(level.getChunkData(STG::LevelFile::kChunkWalls, 1).size() == 152);
	CHECK(level.getChunkData(STG::LevelFile::kChunkWalls, 2).empty());
}

TEST(level_texture_is_column_major) {
	STG::LevelFile level;
	CHECK(loadLevel(level, makeLevelFile(makeSquareLevel())));

	std::vector<uint16> ids = level.getTextureIds();
	CHECK_EQ(ids.size(), (size_t)1);
	CHECK_EQ(ids[0], 9);

	STG::LevelFile::Texture texture;
	CHECK(level.loadTexture(9, texture));
	CHECK_EQ(texture.flags, 0x40);
	CHECK_EQ(texture.surface.w, 2);
	CHECK_EQ(texture.surface.h, 3);
	CHECK_EQ(texture.surface.getPixel(0, 0), 10u);
	CHECK_EQ(texture.surface.getPixel(0, 2), 12u);
	CHECK_EQ(texture.surface.getPixel(1, 0), 13u);
	CHECK(!level.loadTexture(8, texture));
}

TEST(level_rejects_bad_files) {
	STG::LevelFile level;

	std::vector<byte> bytes = makeLevelFile(makeSquareLevel());
	WRITE_LE_UINT32(&bytes[8], 9); // record size 1
	CHECK(!loadLevel(level, bytes));

	bytes = makeLevelFile(makeSquareLevel());
	WRITE_LE_UINT32(&bytes[48 + 4], (uint32)bytes.size()); // chunk 0 offset
	CHECK(!loadLevel(level, bytes));

	std::vector<Chunk> chunks = makeSquareLevel();
	WRITE_LE_UINT16(&chunks[2].data[268], 4); // sector vertex out of range
	CHECK(!loadLevel(level, makeLevelFile(chunks)));

	CHECK(!loadLevel(level, std::vector<byte>(20)));
}

TEST(level_map_draws_sectors) {
	STG::LevelFile level;
	CHECK(loadLevel(level, makeLevelFile(makeSquareLevel())));

	Graphics::Surface map;
	map.create(64, 32, Graphics::PixelFormat::createFormatCLUT8());
	map.fillRect(Common::Rect(map.w, map.h), 0xEE);
	STG::drawLevelMap(level, map);
	// The square fills the middle of the surface; the sides stay clear.
	CHECK(map.getPixel(32, 16) != 0xEEu);
	CHECK_EQ(map.getPixel(2, 16), 0xEEu);
	CHECK_EQ(map.getPixel(61, 16), 0xEEu);
}

namespace {

/**
 * The square level made renderable: a visible floor at height 0 and a wall
 * one unit high along x = 2, both using the 2x3 texture, and a 4x2 sky.
 */
std::vector<Chunk> makeRenderableLevel() {
	std::vector<Chunk> chunks = makeSquareLevel();
	byte *sector = chunks[2].data.data();
	WRITE_LE_UINT32(sector + 108, STG::LevelFile::kFlagVisible);
	WRITE_LE_UINT32(sector + 0x20, 0);
	WRITE_LE_UINT32(sector + 0x48, 0x10000);
	WRITE_LE_UINT32(sector + 0x50, 0x10000);
	WRITE_LE_UINT16(sector + 0x11C, 9);
	WRITE_LE_UINT32(sector + 0x3C, 1); // zone 0
	WRITE_LE_UINT32(sector + 0x74, 1); // only zone 0 in play

	byte *wall = chunks[3].data.data();
	WRITE_LE_UINT32(wall + 0x2C, 0x10000);
	WRITE_LE_UINT32(wall + 0x30, 0);
	WRITE_LE_UINT32(wall + 0x48, 0x20000);
	WRITE_LE_UINT32(wall + 0x50, 0x30000);
	WRITE_LE_UINT32(wall + 0x6C, STG::LevelFile::kFlagVisible);
	WRITE_LE_UINT16(wall + 0x96, 9);
	WRITE_LE_UINT32(wall + 0x3C, 1);

	Chunk billboards = { STG::LevelFile::kChunkBillboards, 1, 1, std::vector<byte>(140) };
	WRITE_LE_UINT32(&billboards.data[0x1C], 0x1CCCD);           // x 1.8
	WRITE_LE_UINT32(&billboards.data[0x20], 0x4000);            // z 0.25
	WRITE_LE_UINT32(&billboards.data[0x24], (uint32)-0x4CCD);   // y -0.3
	WRITE_LE_UINT32(&billboards.data[0x34], 0x2000);
	WRITE_LE_UINT32(&billboards.data[0x38], 0x4000);
	WRITE_LE_UINT16(&billboards.data[0x86], 1);
	WRITE_LE_UINT16(&billboards.data[0x8A], 9);
	WRITE_LE_UINT32(&billboards.data[0x3C], 1);
	chunks.push_back(billboards);

	Chunk sky = { STG::LevelFile::kChunkSky, 0, 1, std::vector<byte>(64 + 8, 0x77) };
	WRITE_LE_UINT32(&sky.data[16], 8);
	WRITE_LE_UINT32(&sky.data[24], 2);
	chunks.push_back(sky);
	return chunks;
}

} // End of anonymous namespace

TEST(level_surfaces_billboards_and_sky) {
	STG::LevelFile level;
	CHECK(loadLevel(level, makeLevelFile(makeRenderableLevel())));

	const STG::LevelFile::Sector &flat = level.getSectors()[0];
	CHECK(!flat.isCeiling());
	CHECK_EQ(flat.texture, 9);
	CHECK_EQ(flat.uScale, 0x10000);

	const STG::LevelFile::Wall &wall = level.getWalls()[0];
	CHECK_EQ(wall.top, 0x10000);
	CHECK_EQ(wall.bottom, 0);
	CHECK_EQ(wall.vLength, 0x30000);
	CHECK_EQ(wall.texture, 9);

	CHECK_EQ(level.getBillboards().size(), (size_t)1);
	CHECK_EQ(level.getBillboards()[0].y, -0x4CCD);
	CHECK_EQ(level.getBillboards()[0].texture, 9);

	Graphics::Surface sky;
	CHECK(level.loadSky(sky));
	CHECK_EQ(sky.w, 4);
	CHECK_EQ(sky.h, 2);
	CHECK_EQ(sky.getPixel(3, 1), 0x77u);
}

TEST(level_renderer_draws_floor_wall_and_sky) {
	STG::LevelFile level;
	CHECK(loadLevel(level, makeLevelFile(makeRenderableLevel())));
	STG::LevelRenderer renderer(level);

	// Standing in the square, looking along +x at the wall.
	STG::Camera camera;
	camera.x = 0.2;
	camera.y = 1;
	camera.z = 0.25;
	Graphics::Surface view;
	view.create(64, 32, Graphics::PixelFormat::createFormatCLUT8());
	renderer.render(camera, view, 0);

	auto isTexel = [](uint32 c) { return c >= 10 && c <= 15; };
	CHECK(isTexel(view.getPixel(32, 31))); // floor
	CHECK(isTexel(view.getPixel(28, 17))); // wall
	// The billboard stands beyond the end of the wall, against the sky.
	CHECK(isTexel(view.getPixel(57, 13)));
	CHECK_EQ(view.getPixel(57, 4), 0x77u);
	CHECK_EQ(view.getPixel(32, 0), 0x77u); // sky above the wall
}

TEST(level_light_levels) {
	STG::LevelFile::LightInfo fixed = { 0, 0, 3 };
	CHECK_EQ(fixed.getLevel(100), 3);
	CHECK_EQ(fixed.getLevel(0), 3);

	// 3200 brightness per level, scaled by the group's percentage.
	STG::LevelFile::LightInfo grouped = { 1, 7680, 0 };
	CHECK_EQ(grouped.getLevel(100), 5);
	CHECK_EQ(grouped.getLevel(50), 6);
	CHECK_EQ(grouped.getLevel(0), 7);
	grouped.brightness = 25600;
	CHECK_EQ(grouped.getLevel(100), 0);
}

TEST(level_shade_tables) {
	Graphics::Palette palette;
	palette.set(0, 0, 0, 0);
	palette.set(1, 128, 128, 128);
	for (int i = 2; i < 256; i++)
		palette.set(i, 255, 255, 255);

	byte tables[STG::LevelFile::kLightLevels][256];
	STG::LevelRenderer::buildShadeTables(palette, 0, 0, 0, tables);
	CHECK_EQ(tables[0][2], 2);
	CHECK_EQ(tables[1][2], 2); // 224: still nearest to white
	CHECK_EQ(tables[4][2], 1); // 127: grey
	CHECK_EQ(tables[7][2], 0); // 31: black
	CHECK_EQ(tables[7][0], 0);
}

TEST(level_blend_table) {
	using STG::LevelRenderer;
	Graphics::Palette palette;
	for (int i = 0; i < 256; i++)
		palette.set(i, 0, 255, 0);
	palette.set(1, 0, 0, 200);
	palette.set(2, 100, 0, 100);
	palette.set(3, 200, 0, 0);
	palette.set(LevelRenderer::kBlendFirst, 200, 0, 0);

	byte table[LevelRenderer::kBlendColors][256];
	LevelRenderer::buildBlendTable(palette, table);
	CHECK_EQ(table[0][1], 2); // red over blue: purple
	CHECK_EQ(table[0][3], 3); // red over red: the first red
}

TEST(level_renderer_lighting) {
	// Put the wall in light group 1 at full brightness. Switching the group
	// off gives light level 7, and with the test palette (black apart from
	// one red entry) every texel then shades to index 0.
	std::vector<Chunk> chunks = makeRenderableLevel();
	WRITE_LE_UINT16(&chunks[3].data[0x64], 1);
	WRITE_LE_UINT16(&chunks[3].data[0x66], 25600);
	STG::LevelFile level;
	CHECK(loadLevel(level, makeLevelFile(chunks)));
	CHECK_EQ(level.getWalls()[0].light.group, 1);

	STG::LevelRenderer renderer(level);
	STG::Camera camera;
	camera.x = 0.2;
	camera.y = 1;
	camera.z = 0.25;
	Graphics::Surface view;
	view.create(64, 32, Graphics::PixelFormat::createFormatCLUT8());

	renderer.render(camera, view, 0);
	CHECK(view.getPixel(28, 17) >= 10);

	renderer.setGroupBrightness(1, 0);
	renderer.render(camera, view, 0);
	CHECK_EQ(view.getPixel(28, 17), 0u);
	CHECK(view.getPixel(32, 31) >= 10); // the floor has no group and stays lit

	renderer.setLightingEnabled(false);
	renderer.render(camera, view, 0);
	CHECK(view.getPixel(28, 17) >= 10);
}

TEST(level_renderer_draws_only_visible_zones) {
	STG::LevelFile level;
	CHECK(loadLevel(level, makeLevelFile(makeRenderableLevel())));
	STG::LevelRenderer renderer(level);
	STG::Camera camera;
	camera.x = 0.2;
	camera.y = 1;
	camera.z = 0.25;
	Graphics::Surface view;
	view.create(64, 32, Graphics::PixelFormat::createFormatCLUT8());

	renderer.setVisibleZones(2); // everything in the level is in zone 0
	renderer.render(camera, view, 0);
	CHECK_EQ(view.getPixel(28, 17), 0x77u);
	CHECK_EQ(view.getPixel(32, 31), 0x77u);
}

namespace {

/** Builds levels of rectangular floors and straight walls for the walker. */
struct LevelBuilder {
	std::vector<std::pair<int32, int32>> vertices;
	std::vector<std::vector<byte>> flats, walls, actors, billboards;

	/** A 2 x 4 billboard standing at (x, y, z). */
	void addBillboard(uint16 id, double x, double y, double z, uint32 flags, uint32 zones) {
		std::vector<byte> r(140);
		WRITE_LE_UINT16(&r[0x14], id);
		WRITE_LE_UINT32(&r[0x1C], (uint32)(int32)(x * 65536));
		WRITE_LE_UINT32(&r[0x20], (uint32)(int32)(z * 65536));
		WRITE_LE_UINT32(&r[0x24], (uint32)(int32)(y * 65536));
		WRITE_LE_UINT32(&r[0x34], 1 << 16);
		WRITE_LE_UINT32(&r[0x38], 4 << 16);
		WRITE_LE_UINT32(&r[0x3C], zones);
		WRITE_LE_UINT32(&r[0x6C], flags);
		billboards.push_back(r);
	}

	/**
	 * An actor of sprite set 1 standing at (x, y, z). Animation 0 (stand)
	 * has one frame in eight mirrored directions; animation 6 (walk) four
	 * frames in one direction at 1 unit per frame.
	 */
	std::vector<byte> &addActor(uint16 id, double x, double y, double z, int mode, uint16 heading) {
		std::vector<byte> r(276);
		WRITE_LE_UINT16(&r[0x14], id);
		WRITE_LE_UINT32(&r[0x1C], (uint32)(int32)(x * 65536));
		WRITE_LE_UINT32(&r[0x20], (uint32)(int32)(z * 65536));
		WRITE_LE_UINT32(&r[0x24], (uint32)(int32)(y * 65536));
		WRITE_LE_UINT32(&r[0x28], 2 << 16); // reach
		WRITE_LE_UINT32(&r[0x34], 1 << 16); // scale
		WRITE_LE_UINT32(&r[0x38], 6 << 16); // height
		WRITE_LE_UINT32(&r[0x54], 1 << 16); // radius
		WRITE_LE_UINT32(&r[0x58], 6 << 16); // body height
		WRITE_LE_UINT32(&r[0xAC], 2 << 16); // step height
		const byte stand[8] = { 0, 0, 0, 1, 0, 8, 1, 0 };
		memcpy(&r[0xC4], stand, 8);
		const byte walk[8] = { 0, 1, 64, 4, 0, 1, 0, 1 };
		memcpy(&r[0xC4 + 6 * 8], walk, 8);
		WRITE_LE_UINT16(&r[0xFC], 1);
		WRITE_LE_UINT16(&r[0x104], 0xFFFF); // turn speed
		WRITE_LE_UINT16(&r[0x108], heading);
		r[0x10E] = (byte)mode;
		actors.push_back(r);
		return actors.back();
	}

	uint16 addVertex(double x, double y) {
		vertices.push_back({ (int32)(x * 65536), (int32)(y * 65536) });
		return (uint16)(vertices.size() - 1);
	}

	void addFloor(double x0, double y0, double x1, double y1, double height, uint32 zone = 1, uint32 active = 1,
	              uint32 flags = 0) {
		std::vector<byte> r(292);
		WRITE_LE_UINT32(&r[108], STG::LevelFile::kFlagVisible | flags);
		WRITE_LE_UINT32(&r[0x20], (uint32)(int32)(height * 65536));
		WRITE_LE_UINT32(&r[0x3C], zone);
		WRITE_LE_UINT32(&r[0x74], active);
		WRITE_LE_UINT16(&r[266], 4);
		WRITE_LE_UINT16(&r[268], addVertex(x0, y0));
		WRITE_LE_UINT16(&r[270], addVertex(x1, y0));
		WRITE_LE_UINT16(&r[272], addVertex(x1, y1));
		WRITE_LE_UINT16(&r[274], addVertex(x0, y1));
		flats.push_back(r);
	}

	/** A wall from (x0, y0) to (x1, y1), solid on its left as seen walking along it. */
	void addWall(double x0, double y0, double x1, double y1, double bottom, double top, uint32 flags = 0,
	             uint32 zone = 1, uint16 objectId = 0) {
		std::vector<byte> r(152);
		WRITE_LE_UINT16(&r[0x14], objectId);
		double dx = x1 - x0, dy = y1 - y0, length = std::sqrt(dx * dx + dy * dy);
		double nx = -dy / length, ny = dx / length, d = -(nx * x0 + ny * y0);
		WRITE_LE_UINT32(&r[0x1C], (uint32)(int32)(nx * 65536));
		WRITE_LE_UINT32(&r[0x24], (uint32)(int32)(ny * 65536));
		WRITE_LE_UINT32(&r[0x28], (uint32)(int32)(d * 65536));
		WRITE_LE_UINT32(&r[0x2C], (uint32)(int32)(top * 65536));
		WRITE_LE_UINT32(&r[0x30], (uint32)(int32)(bottom * 65536));
		WRITE_LE_UINT32(&r[0x3C], zone);
		WRITE_LE_UINT32(&r[0x6C], STG::LevelFile::kFlagVisible | flags);
		WRITE_LE_UINT16(&r[0x86], addVertex(x0, y0));
		WRITE_LE_UINT16(&r[0x88], addVertex(x1, y1));
		walls.push_back(r);
	}

	std::vector<byte> build() const {
		std::vector<Chunk> chunks;
		chunks.push_back({ STG::LevelFile::kChunkPalette, 1, 256, std::vector<byte>(1024) });
		Chunk v = { STG::LevelFile::kChunkVertices, 1, (uint16)vertices.size(), {} };
		for (const auto &p : vertices) {
			byte b[8];
			WRITE_LE_UINT32(b, (uint32)p.first);
			WRITE_LE_UINT32(b + 4, (uint32)p.second);
			v.data.insert(v.data.end(), b, b + 8);
		}
		chunks.push_back(v);
		Chunk f = { STG::LevelFile::kChunkSectors, 1, (uint16)flats.size(), {} };
		for (const auto &r : flats)
			f.data.insert(f.data.end(), r.begin(), r.end());
		chunks.push_back(f);
		Chunk w = { STG::LevelFile::kChunkWalls, 1, (uint16)walls.size(), {} };
		for (const auto &r : walls)
			w.data.insert(w.data.end(), r.begin(), r.end());
		chunks.push_back(w);
		Chunk b = { STG::LevelFile::kChunkBillboards, 1, (uint16)billboards.size(), {} };
		for (const auto &r : billboards)
			b.data.insert(b.data.end(), r.begin(), r.end());
		chunks.push_back(b);
		Chunk a = { STG::LevelFile::kChunkActors, 1, (uint16)actors.size(), {} };
		for (const auto &r : actors)
			a.data.insert(a.data.end(), r.begin(), r.end());
		chunks.push_back(a);
		return makeLevelFile(chunks);
	}
};

/** A 20 x 20 floor at height 0, with a wall along x = 10 facing -x. */
LevelBuilder makeRoom(double wallTop = 12, uint32 wallFlags = 0, uint32 wallZone = 1) {
	LevelBuilder builder;
	builder.addFloor(0, 0, 20, 20, 0);
	builder.addWall(10, 0, 10, 20, 0, wallTop, wallFlags, wallZone);
	return builder;
}

} // End of anonymous namespace

TEST(walker_stops_at_walls) {
	STG::LevelFile level;
	CHECK(loadLevel(level, makeRoom().build()));
	STG::Walker walker(level);
	walker.place(5, 10, 0);
	CHECK_EQ(walker.getFloor(), 0);
	CHECK(walker.getFeet() == 0);
	CHECK(walker.getEye() == STG::Walker::kEyeHeight);

	walker.move(10, 0, 0.1);
	CHECK(std::fabs(walker.getX() - (10 - STG::Walker::kRadius)) < 0.05);
	CHECK(std::fabs(walker.getY() - 10) < 1e-9);

	// Pushing into the wall at an angle slides along it.
	walker.move(5, 4, 0.1);
	CHECK(walker.getX() < 10 - STG::Walker::kRadius + 0.01);
	CHECK(std::fabs(walker.getY() - 14) < 0.05);

	// Walking away is never blocked.
	walker.move(-3, 0, 0.1);
	CHECK(std::fabs(walker.getX() - (7 - STG::Walker::kRadius)) < 0.05);

	walker.setCollisionEnabled(false);
	walker.move(10, 0, 0.1);
	CHECK(walker.getX() > 15);
}

TEST(walker_wall_sides_heights_and_zones) {
	// From behind, a one-sided wall lets the player through...
	{
		STG::LevelFile level;
		CHECK(loadLevel(level, makeRoom().build()));
		STG::Walker walker(level);
		walker.place(15, 10, 0);
		walker.move(-10, 0, 0.1);
		CHECK(std::fabs(walker.getX() - 5) < 1e-9);
	}
	// ...but one solid on both sides does not.
	{
		STG::LevelFile level;
		CHECK(loadLevel(level, makeRoom(12, STG::LevelFile::kWallSolidBothSides).build()));
		STG::Walker walker(level);
		walker.place(15, 10, 0);
		walker.move(-10, 0, 0.1);
		CHECK(std::fabs(walker.getX() - (10 + STG::Walker::kRadius)) < 0.05);
	}
	// A wall lower than the step height is stepped over.
	{
		STG::LevelFile level;
		CHECK(loadLevel(level, makeRoom(1.5).build()));
		STG::Walker walker(level);
		walker.place(5, 10, 0);
		walker.move(10, 0, 0.1);
		CHECK(std::fabs(walker.getX() - 15) < 1e-9);
	}
	// A wall in a zone that isn't in play is ignored.
	{
		STG::LevelFile level;
		CHECK(loadLevel(level, makeRoom(12, 0, 2).build()));
		STG::Walker walker(level);
		walker.place(5, 10, 0);
		CHECK(walker.getActiveZones() == 1);
		walker.move(10, 0, 0.1);
		CHECK(std::fabs(walker.getX() - 15) < 1e-9);
	}
}

TEST(walker_steps_up_and_falls) {
	LevelBuilder builder;
	builder.addFloor(0, 0, 10, 10, 0);
	builder.addFloor(10, 0, 20, 10, 1.5);  // a step up
	builder.addFloor(20, 0, 30, 10, 5);    // too high to step onto
	builder.addFloor(0, 10, 10, 20, -10);  // a drop
	STG::LevelFile level;
	CHECK(loadLevel(level, builder.build()));
	STG::Walker walker(level);

	walker.place(5, 5, 0);
	walker.move(10, 0, 0.1);
	CHECK_EQ(walker.getFloor(), 1);
	CHECK(walker.getFeet() == 1.5);

	// The higher floor can't be stepped onto. Under it there is no floor
	// at all, so the player keeps their height and every zone opens up.
	walker.move(10, 0, 0.1);
	CHECK_EQ(walker.getFloor(), -1);
	CHECK(walker.getFeet() == 1.5);
	CHECK(walker.getActiveZones() == STG::Walker::kAllZones);

	walker.place(5, 5, 0);
	walker.move(0, 10, 0.1);
	CHECK_EQ(walker.getFloor(), 3);
	CHECK(walker.getFeet() < 0);
	CHECK(walker.getFeet() > -10);
	for (int i = 0; i < 50; i++)
		walker.move(0, 0, 0.05);
	CHECK(walker.getFeet() == -10);
}

namespace {

/** Wrap program words in an SGEN file with empty sections. */
std::vector<byte> makeScriptFile(const std::vector<uint16> &program) {
	std::vector<byte> file(40);
	WRITE_BE_UINT32(&file[0], MKTAG('S', 'G', 'E', 'N'));
	WRITE_LE_UINT16(&file[4], 22);
	WRITE_LE_UINT16(&file[6], 2);
	WRITE_LE_UINT32(&file[12], (uint32)program.size() * 2);
	for (uint16 word : program) {
		file.push_back((byte)word);
		file.push_back((byte)(word >> 8));
	}
	WRITE_LE_UINT32(&file[8], (uint32)file.size() - 12);
	return file;
}

} // End of anonymous namespace

TEST(mission_runs_level_scripts) {
	using namespace STG;
	const uint16 P = OP_PUSH;
	std::vector<uint16> program = {
		// A logic object: subscribes to broadcasts and starts a 100 ms timer
		// on entering state 0, counts timer ticks in global 0, stores signals
		// in global 1, and puts the player at (5, 10) facing +x on the start
		// broadcast.
		1, 0xE001,
		0x8000,
		kEventEnterState, 0xFFFF, OP_PUSH_SELF, 0x31, P, 100, OP_PUSH_SELF, 0x32, OP_END,
		Mission::kEventStart, 0xFFFF, OP_PUSH_BLOCK, 90, 0, 0, 0, 10, 0, 5, 0x75, OP_END,
		Mission::kEventTimer, 0xFFFF, P, 0, OP_GET_GLOBAL, OP_INC, P, 0, OP_SET_GLOBAL, OP_END,
		Mission::kEventSignal, 0xFFFF, P, 1, OP_SET_GLOBAL, OP_END,
		0x3000,
		0x2000,
		// The wall with id 5: when touched, sets global 2, signals 77 to the
		// logic object and hides itself.
		2, 0x4005,
		0x8000,
		Mission::kEventTouch, 0xFFFF, P, 1, P, 2, OP_SET_GLOBAL, P, 77, P, 0xE001, OP_SEND_SIGNAL, OP_PUSH_SELF,
		0x46, OP_END,
		0x3000,
		0x2000,
		0x1000
	};
	std::vector<byte> scriptBytes = makeScriptFile(program);
	Common::MemoryReadStream scriptStream(scriptBytes.data(), (uint32)scriptBytes.size());
	ScriptFile script;
	CHECK(script.load(scriptStream));

	LevelBuilder builder;
	builder.addFloor(0, 0, 20, 20, 0);
	builder.addWall(10, 0, 10, 20, 0, 12, 0, 1, 5);
	LevelFile level;
	CHECK(loadLevel(level, builder.build()));
	LevelRenderer renderer(level);
	Walker walker(level);
	walker.place(2, 2, 0);

	Mission mission(level, script, renderer, walker, nullptr);
	mission.start();
	CHECK(walker.getX() == 5);
	CHECK(walker.getY() == 10);
	double yaw = 1;
	CHECK(mission.takePlayerYaw(yaw));
	CHECK(std::fabs(yaw) < 1e-9);

	mission.update(250);
	CHECK_EQ(mission.getVM().getGlobal(0), 2);

	// Walking into the wall tells its object, which hides it.
	walker.move(10, 0, 0.01);
	CHECK(walker.getX() < 10);
	CHECK_EQ(walker.getContactWall(), 0);
	mission.update(10);
	CHECK_EQ(mission.getVM().getGlobal(2), 1);
	CHECK_EQ(mission.getVM().getGlobal(1), 77);
	CHECK(!(level.getWalls()[0].flags & LevelFile::kFlagVisible));

	walker.move(10, 0, 0.01);
	CHECK(walker.getX() > 15);
	CHECK(mission.getMissingNatives().empty());
}

TEST(mission_items_rest_on_floors) {
	using namespace STG;
	const uint16 P = OP_PUSH;
	// On the start broadcast: item 8 is dropped at the player, and item 7
	// is told to move, which items ignore.
	std::vector<uint16> program = {
		1, 0xE001,
		0x8000,
		kEventEnterState, 0xFFFF, OP_PUSH_SELF, 0x31, OP_END,
		Mission::kEventStart, 0xFFFF, P, 0, P, 0x2008, 0x71,
		OP_PUSH_BLOCK, 0, 0, 0, 35, 0, 35, P, 0x2007, 0x4D, OP_END,
		0x3000,
		0x2000,
		0x1000
	};
	std::vector<byte> scriptBytes = makeScriptFile(program);
	Common::MemoryReadStream scriptStream(scriptBytes.data(), (uint32)scriptBytes.size());
	ScriptFile script;
	CHECK(script.load(scriptStream));

	LevelBuilder builder;
	builder.addFloor(0, 0, 20, 20, 0, 1, 1);
	builder.addFloor(20, 0, 40, 40, 3, 2, 2);
	builder.addFloor(0, 0, 10, 10, 1, 1, 1, LevelFile::kFlagNoActors);
	// Items wait in a storage zone until they are placed.
	const uint32 item = LevelFile::kBillboardItem | LevelFile::kFlagVisible;
	builder.addBillboard(7, 5, 5, 1.5, item, 0x80);
	builder.addBillboard(8, 100, 100, 0, item, 0x80);
	LevelFile level;
	CHECK(loadLevel(level, builder.build()));
	LevelRenderer renderer(level);
	Walker walker(level);
	walker.place(30, 30, 3);

	Mission mission(level, script, renderer, walker, nullptr);
	// At the start: item 7 settles past the floor actors don't use onto
	// the one below, and takes its zones. Item 8 has no floor under it.
	const LevelFile::Billboard &seven = level.getBillboards()[0], &eight = level.getBillboards()[1];
	CHECK(seven.z == 0);
	CHECK_EQ(seven.zones, (uint64)1);
	CHECK(eight.z == 0);
	CHECK_EQ(eight.zones, (uint64)0x80);

	mission.start();
	CHECK(eight.x == 30 << 16);
	CHECK(eight.y == 30 << 16);
	CHECK(eight.z == 3 << 16);
	CHECK_EQ(eight.zones, (uint64)2);
	CHECK(seven.x == 5 << 16);
	CHECK(mission.getMissingNatives().empty());
}

namespace {

/**
 * A sprite set of one-pixel frames whose colour is 10 + the frame number:
 * sequence 0 (stand) is frames 0-4, one per view direction, and sequence 1
 * (walk) frames 5-8.
 */
bool loadTestSprites(uint16 number, STG::SpriteSet &set) {
	Common::MemoryWriteStreamDynamic file;
	file.writeUint32LE(2);
	file.writeUint32LE(9);
	file.writeUint32LE(0);
	file.writeUint32LE(2);
	file.writeUint32LE(0);
	file.writeUint32LE(0);
	const byte sequences[] = { 5, 0, 1, 2, 3, 4, 4, 5, 6, 7, 8 };
	file.write(sequences, sizeof(sequences));
	for (int i = 0; i < 9; i++) {
		file.writeUint32LE(1 << 16); // half width 1.0
		file.writeUint32LE(1);
		file.writeUint32LE(1);
		const byte pixel[4] = { (byte)(10 + i), 0, 0, 0 };
		file.write(pixel, sizeof(pixel));
	}
	std::vector<byte> data = file.getData();
	Common::MemoryReadStream stream(data.data(), (uint32)data.size());
	return set.load(stream);
}

} // End of anonymous namespace

TEST(actors_headings_and_view_directions) {
	using STG::Actors;
	CHECK_EQ(Actors::headingTowards(0, 1), 0);
	CHECK_EQ(Actors::headingTowards(-1, 0), 16384);
	CHECK_EQ(Actors::headingTowards(1, 0), 49152);

	LevelBuilder builder;
	builder.addFloor(0, 0, 40, 40, 0);
	builder.addActor(1, 10, 10, 0, Actors::kModeIdle, 0); // facing +y
	STG::LevelFile level;
	CHECK(loadLevel(level, builder.build()));
	STG::Walker walker(level);
	Actors actors(level, walker, loadTestSprites);

	std::vector<STG::SpriteDraw> sprites;
	auto viewFrom = [&](double x, double y, bool &mirrored) {
		actors.getSprites(x, y, STG::Walker::kAllZones, sprites);
		mirrored = !sprites.empty() && sprites[0].mirrored;
		return sprites.empty() ? -1 : (int)sprites[0].surface->getPixel(0, 0) - 10;
	};
	bool mirrored;
	CHECK_EQ(viewFrom(10, 20, mirrored), 4); // in front
	CHECK_EQ(viewFrom(10, 0, mirrored), 0); // behind
	CHECK_EQ(viewFrom(20, 10, mirrored), 2); // from its right
	CHECK(!mirrored);
	CHECK_EQ(viewFrom(0, 10, mirrored), 2); // from its left: the right view flipped
	CHECK(mirrored);
	CHECK(sprites[0].halfWidth == 1.0);
	CHECK(sprites[0].height == 6.0);
	CHECK(sprites[0].bottom == 0.0);

	actors.setMode(0xA001, Actors::kModeHidden);
	viewFrom(10, 20, mirrored);
	CHECK(sprites.empty());
}

TEST(actors_walk_and_block) {
	using STG::Actors;
	LevelBuilder builder;
	builder.addFloor(0, 0, 40, 40, 0);
	builder.addActor(1, 10, 10, 0, Actors::kModeWalkTo, 49152); // facing +x
	builder.addActor(2, 10, 30, 0, Actors::kModeIdle, 0);
	STG::LevelFile level;
	CHECK(loadLevel(level, builder.build()));
	STG::Walker player(level);
	player.place(30, 10, 0);
	Actors actors(level, player, loadTestSprites);
	player.setObstacles(&actors.getObstacles());

	// Actor 1 walks at the player: 64 frames a second at 1 unit per frame.
	std::vector<Actors::ScriptEvent> events;
	int arrived = 0, cycles = 0;
	for (int i = 0; i < 15; i++) {
		events.clear();
		actors.update(20, player, events);
		for (const Actors::ScriptEvent &event : events) {
			CHECK_EQ(event.object, 0xA001);
			arrived += event.event == Actors::kEventArrived;
			cycles += event.event == Actors::kEventCycle;
		}
	}
	double x, y, z;
	uint16 heading;
	CHECK(actors.getPosition(1, x, y, z, heading));
	CHECK(x > 20 && x < 30);
	CHECK(std::fabs(y - 10) < 0.01);
	CHECK_EQ(heading, 49152);
	CHECK(cycles >= 3);
	CHECK_EQ(arrived, 0);
	for (int i = 0; i < 20 && !arrived; i++) {
		events.clear();
		actors.update(20, player, events);
		for (const Actors::ScriptEvent &event : events)
			arrived += event.event == Actors::kEventArrived;
	}
	CHECK(arrived > 0);

	// Actor 2 stands in the player's way.
	player.place(10, 22, 0);
	player.move(0, 10, 0.1);
	CHECK(std::fabs(player.getY() - (30 - 1 - STG::Walker::kRadius)) < 0.05);
	CHECK_EQ(actors.getObstacleObject(player.getContactObstacle()), 0xA002);
}
