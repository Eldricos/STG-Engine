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
