/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "test/test.h"

#include "common/endian.h"
#include "common/stream.h"
#include "engines/stg/level.h"
#include "engines/stg/render3d.h"

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

	byte *wall = chunks[3].data.data();
	WRITE_LE_UINT32(wall + 0x2C, 0x10000);
	WRITE_LE_UINT32(wall + 0x30, 0);
	WRITE_LE_UINT32(wall + 0x48, 0x20000);
	WRITE_LE_UINT32(wall + 0x50, 0x30000);
	WRITE_LE_UINT32(wall + 0x6C, STG::LevelFile::kFlagVisible);
	WRITE_LE_UINT16(wall + 0x96, 9);

	Chunk billboards = { STG::LevelFile::kChunkBillboards, 1, 1, std::vector<byte>(140) };
	WRITE_LE_UINT32(&billboards.data[0x1C], 0x1CCCD);           // x 1.8
	WRITE_LE_UINT32(&billboards.data[0x20], 0x4000);            // z 0.25
	WRITE_LE_UINT32(&billboards.data[0x24], (uint32)-0x4CCD);   // y -0.3
	WRITE_LE_UINT32(&billboards.data[0x34], 0x2000);
	WRITE_LE_UINT32(&billboards.data[0x38], 0x4000);
	WRITE_LE_UINT16(&billboards.data[0x86], 1);
	WRITE_LE_UINT16(&billboards.data[0x8A], 9);
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

	double floor = -1;
	CHECK(renderer.findFloor(1, 1, 10, floor));
	CHECK(floor == 0);
	CHECK(!renderer.findFloor(5, 1, 10, floor));
	CHECK(!renderer.findFloor(1, 1, -1, floor));

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
