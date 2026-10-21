/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_LEVEL_H
#define STG_LEVEL_H

#include <vector>

#include "common/graphics/palette.h"
#include "common/graphics/surface.h"
#include "common/stream.h"

namespace STG {

/**
 * A 3D level (GAME.LV2), from the loader at 0x4BD130 in sgens.exe.
 *
 *   uint32   1 (version)
 *   uint32   record sizes[9]: 1024, 8, 292, 152, 140, 276, 196, 64, 24.
 *            The original refuses files whose sizes differ from its own.
 *   uint32   directory entry count
 *   uint32   directory size in bytes (count * 16)
 *   entries: uint32 size, uint32 file offset, uint16 type, uint16 id,
 *            uint16 count A, uint16 count B
 *   chunk data
 *
 * Chunks are loaded on demand by (type, id). The level uses id 1 of the
 * table types, where count B is the number of records:
 *
 *   type 1   palette: 256 x (R, G, B, 0)
 *   type 2   vertices: (x, y) in 16.16 fixed point, the floor plan
 *   type 3   sectors (292 bytes): convex polygons of 3-8 vertices that
 *            tile the map, possibly stacked above one another
 *   type 4   walls (152 bytes): a line between two vertices
 *   type 5-7 further polygon records (140, 276, 196 bytes), unexplored
 *   type 12  one 161 KB chunk, unexplored
 *   type 13  textures, one chunk each, id = texture number
 *
 * Every byte of verid1's file belongs to the header, directory or a chunk.
 */
class LevelFile {
public:
	enum ChunkType {
		kChunkPalette = 1,
		kChunkVertices = 2,
		kChunkSectors = 3,
		kChunkWalls = 4,
		kChunkTexture = 13
	};

	static const uint32 kVersion = 1;
	static const int kRecordSizeCount = 9;
	static const uint32 kRecordSizes[kRecordSizeCount];
	/** Palette index of the transparent colour (magenta) in textures. */
	static const byte kTransparent = 0xFF;

	struct ChunkEntry {
		uint32 size;
		uint32 offset;
		uint16 type;
		uint16 id;
		uint16 countA;
		uint16 countB;
	};

	/** Floor plan coordinates, 16.16 fixed point. */
	struct Vertex {
		int32 x;
		int32 y;
	};

	struct Wall {
		uint16 vertex1;
		uint16 vertex2;
		/**
		 * Line equation: normalX * x + normalY * y + distance = 0, with a unit
		 * normal (16.16), from offsets 28, 36 and 40. Offset 32 holds a
		 * further value whose meaning is unknown (possibly a slope).
		 */
		int32 normalX;
		int32 normalY;
		int32 distance;
	};

	struct Sector {
		/** Vertex indices of the convex polygon, from offsets 266/268. */
		std::vector<uint16> vertices;
		/** Flags word at offset 108. */
		uint32 flags;
		/** The raw 292-byte record, for the fields still being decoded. */
		std::vector<byte> raw;
	};

	struct Texture {
		uint16 id = 0;
		uint16 flags = 0;
		/** Upright image; pixels equal to kTransparent are see-through. */
		Graphics::Surface surface;
	};

	bool load(Common::SeekableReadStream &stream);

	const std::vector<ChunkEntry> &getDirectory() const { return _directory; }
	const ChunkEntry *findChunk(uint16 type, uint16 id) const;
	/** A chunk's bytes, or an empty vector if it is missing. */
	std::vector<byte> getChunkData(uint16 type, uint16 id) const;

	const Graphics::Palette &getPalette() const { return _palette; }
	const std::vector<Vertex> &getVertices() const { return _vertices; }
	const std::vector<Wall> &getWalls() const { return _walls; }
	const std::vector<Sector> &getSectors() const { return _sectors; }

	/** Ids of all texture chunks, in directory order. */
	std::vector<uint16> getTextureIds() const;
	bool loadTexture(uint16 id, Texture &texture) const;

private:
	bool loadTables();

	std::vector<byte> _data;
	std::vector<ChunkEntry> _directory;
	Graphics::Palette _palette;
	std::vector<Vertex> _vertices;
	std::vector<Wall> _walls;
	std::vector<Sector> _sectors;
};

/**
 * Draw a level's floor plan from above into dst: sectors filled, walls on
 * top, scaled to fit the surface. Colour indices refer to the level palette.
 */
void drawLevelMap(const LevelFile &level, Graphics::Surface &dst);

} // End of namespace STG

#endif
