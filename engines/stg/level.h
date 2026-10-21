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
 *   type 3   flats (292 bytes): horizontal convex polygons of 3-8
 *            vertices, each a floor or a ceiling at one height
 *   type 4   walls (152 bytes): vertical quads between two vertices
 *   type 5   billboards (140 bytes): sprites that always face the camera
 *   type 6   actors (276 bytes), placed and moved by the scripts
 *   type 7   (196 bytes), empty in verid1
 *   type 12  the sky: a 64-byte header and a row-major image
 *   type 13  textures, one chunk each, id = texture number
 *
 * Every byte of verid1's file belongs to the header, directory or a chunk.
 *
 * World space: x and y are the floor plan, z is height and grows upwards.
 * All coordinates are 16.16 fixed point. Fields were identified from the
 * renderer in sgens.exe (0x4C13F0 flats, 0x4C7D00 walls).
 */
class LevelFile {
public:
	enum ChunkType {
		kChunkPalette = 1,
		kChunkVertices = 2,
		kChunkSectors = 3,
		kChunkWalls = 4,
		kChunkBillboards = 5,
		kChunkSky = 12,
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

	/**
	 * How a wall, flat or billboard is lit, from offsets 0x64-0x68.
	 *
	 * The renderer darkens each surface by one of 8 levels, 0 being fully
	 * lit and 7 nearly black (see buildShadeTables()). A surface in a light
	 * group gets its level from its brightness and the group's brightness,
	 * which the scripts can change; any other surface keeps the level
	 * stored in the file.
	 */
	struct LightInfo {
		/** Light group, or 0. */
		int16 group;
		/** Brightness when the group is fully on; 3200 per light level. */
		uint16 brightness;
		/** Fixed light level, used when group is 0. */
		byte level;

		/** The light level for a group brightness of percent (100 = on). */
		int getLevel(int percent) const;
	};

	static const int kLightLevels = 8;

	struct Wall {
		uint16 vertex1;
		uint16 vertex2;
		/**
		 * Line equation: normalX * x + normalY * y + distance = 0, with a unit
		 * normal (16.16), from offsets 28, 36 and 40. The renderer uses it
		 * to cull walls facing away unless flag 0xC0 (two-sided) is set.
		 */
		int32 normalX;
		int32 normalY;
		int32 distance;
		/** Heights of the top and bottom edges. */
		int32 top;
		int32 bottom;
		/**
		 * Texture coordinates in texels (16.16): u runs from uStart at
		 * vertex1 to uStart + uLength at vertex2, v from vStart at the top
		 * to vStart + vLength at the bottom.
		 */
		int32 uStart;
		int32 uLength;
		int32 vStart;
		int32 vLength;
		uint16 texture;
		/** Animation frames: textures texture .. texture + frames - 1. */
		uint16 frames;
		uint32 flags;
		LightInfo light;
		/** Zones the wall belongs to (offset 0x3C); see Sector::activeZones. */
		uint64 zones;
	};

	/** Wall flag: blocks movement from behind as well as from the front. */
	static const uint32 kWallSolidBothSides = 0x8000;

	/**
	 * A floor or ceiling polygon. The original stores these in the same
	 * table and draws a floor only from above and a ceiling from below.
	 */
	struct Sector {
		/** Vertex indices of the convex polygon, from offsets 266/268. */
		std::vector<uint16> vertices;
		/** Flags word at offset 108; see kFlag* below. */
		uint32 flags;
		int32 height;
		/**
		 * World-aligned texture mapping in texels (16.16):
		 * u = x * uScale + uOffset, v = y * vScale + vOffset, rotated by
		 * rotation (a 16-bit angle) when that is non-zero.
		 */
		int32 uOffset;
		int32 uScale;
		int32 vOffset;
		int32 vScale;
		uint16 rotation;
		uint16 texture;
		uint16 frames;
		LightInfo light;
		/** The zone the flat belongs to (offset 0x3C, one bit set). */
		uint64 zones;
		/**
		 * The zones in play while standing on this floor (offset 0x74).
		 * Only walls, flats and billboards in those zones are drawn and
		 * collided with, which keeps rooms stacked above one another apart.
		 */
		uint64 activeZones;
		/** The raw 292-byte record, for the fields still being decoded. */
		std::vector<byte> raw;

		bool isCeiling() const { return (flags & kFlagCeiling) != 0; }
	};

	static const uint32 kFlagVisible = 0x100;
	static const uint32 kFlagCeiling = 0x100000;

	/** A camera-facing sprite, standing on (x, y, z). */
	struct Billboard {
		int32 x;
		int32 y;
		int32 z;
		/** Half the width, and the height. */
		int32 halfWidth;
		int32 height;
		uint16 texture;
		uint16 frames;
		uint32 flags;
		LightInfo light;
		uint64 zones;
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
	const std::vector<Billboard> &getBillboards() const { return _billboards; }

	/**
	 * The sky image, if the level has one. The header holds the image size
	 * at offset 16 and its height at offset 24; rows are stored in order.
	 */
	bool loadSky(Graphics::Surface &sky) const;

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
	std::vector<Billboard> _billboards;
};

/**
 * Draw a level's floor plan from above into dst: sectors filled, walls on
 * top, scaled to fit the surface. Colour indices refer to the level palette.
 */
void drawLevelMap(const LevelFile &level, Graphics::Surface &dst);

} // End of namespace STG

#endif
