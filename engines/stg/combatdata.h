/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_COMBATDATA_H
#define STG_COMBATDATA_H

#include <vector>

#include "common/str.h"
#include "common/stream.h"
#include "common/types.h"

namespace STG {

/**
 * The space combat's files in COMBAT.DAT, which come from a separate 3D
 * library ("DRE") and its own tools, unlike the rest of the game.
 */

/**
 * A model (.3DV, .3DR, .PCn), DRE object format v1.10 (loader 0x4E4C30):
 * parts of textured triangles, x across, y up, z forward.
 *
 *   u32 size (of the rest), u16 version 110, u16 max verts, u16 max faces, char name[20]
 *   then a block whose offsets count from its start:
 *     +04 i32 parts, +08 u32 offset of the 44-byte part records,
 *     +10 f32 extent[3], +1C f32 centre[3]
 *   part: i32 verts (negative: a billboard quad), i32 faces, i32 2,
 *     u32 offsets of verts (f32[3]), uvs (16.16 pairs, or 0), 0, 0,
 *     faces (u16 material, a, b, c), vertex normals, face normals (f32[3]),
 *     plane distances (f32)
 */
struct Model3D {
	struct Vertex {
		float x, y, z;
		/** Texture coordinates, 0..1 across the texture. */
		float u, v;
	};
	struct Triangle {
		uint16 material, a, b, c;
		float nx, ny, nz;
	};
	struct Part {
		std::vector<Vertex> vertices;
		std::vector<Triangle> triangles;
		bool textured = false;
	};
	std::vector<Part> parts;
	float extent[3] = {}, centre[3] = {};
	/** Half the largest extent. */
	float radius = 0;

	bool load(Common::SeekableReadStream &stream);
};

/** A model's materials (.MTL, .MTR, .DMG; loader 0x4E5520), 36 bytes each. */
struct CombatMaterial {
	/** RGB555, for untextured faces. */
	uint32 colour = 0;
	uint32 param = 0;
	/** 1 textured, 2 an effect (black see-through), 3 flat, 0 flat see-through. */
	uint32 flags = 0;
	Common::String texture;

	static bool loadList(Common::SeekableReadStream &stream, std::vector<CombatMaterial> &list);
	/** The colour as 0xAARRGGBB. */
	uint32 rgb() const;
};

/** A true-colour picture: textures (.IMG = GIF, .LBM) and the console's .RAS. */
struct CombatImage {
	int w = 0, h = 0;
	/** 0xAARRGGBB; alpha 0 where see-through. */
	std::vector<uint32> pixels;

	/** A GIF87a/89a (the .IMG textures). */
	bool loadGif(Common::SeekableReadStream &stream);
	/** An IFF PBM (.LBM: BOOM, PHOTON, PHASER1, SHIELD). */
	bool loadLbm(Common::SeekableReadStream &stream);
	/**
	 * A Sun rasterfile (.RAS, loader 0x49C23E), 8-bit with its own colour
	 * map in three planes; colour (255,0,163) is see-through.
	 */
	bool loadRas(Common::SeekableReadStream &stream);
	uint32 get(int x, int y) const { return pixels[(size_t)y * w + x]; }
};

/** The palette of the animations and the starfield (STANDARD.PAL, 6-bit VGA). */
bool loadStandardPalette(Common::SeekableReadStream &stream, uint32 palette[256]);

/**
 * An animation (.SPR, anim.cpp 0x4AF7B0): explosions, shield hits,
 * torpedoes and phaser beams, in STANDARD.PAL. Chunks of 'SPRT', 'LIST',
 * 'TIME' (ms a frame) and 'COMP' (a frame, packed one of four ways).
 */
struct SprAnimation {
	int w = 0, h = 0;
	uint32 frameMillis = 100;
	/** Palette indices; 0 is see-through. */
	std::vector<std::vector<byte>> frames;

	bool load(Common::SeekableReadStream &stream);
};

/** A ship class from SYSTEMS.DAT (parser 0x49C770). */
struct ShipClass {
	enum System {
		kShip = 0,
		kShield1 = 1, // .. kShield4 = 4
		kWeapons = 5,
		kLifeSupport = 6,
		kEngines = 7,
		kPower = 8,
		kComputer = 9,
		kSensors = 10,
		kDamageControl = 11,
		kCloak = 12,
		kPhaser0 = 13, // .. 22
		kPhoton0 = 23, // .. 26
		kSystemCount = 27
	};
	struct SystemInfo {
		/** -1: the ship doesn't have it. Photon tubes: torpedoes a salvo. */
		int max = -1;
		int charge = 0, discharge = 0, repair = 0;
		/** Systems 1..12: cumulative threshold for random hits, and % for an aimed hit. */
		int threshold = 0, aimed = 0;
		/** Weapons: the octants (0..7) the bank or tube fires into. */
		int arcs = 0;
	};
	Common::String token;
	int maxSpeed = 500, agility = 50, hitPoints = 100, closeRadius = 5000, destruct = 50;
	int phasers = 0, photons = 0, shields = 0, torpedoes = 0;
	SystemInfo systems[kSystemCount];
	/** % of a hit leaking past each shield, by shield energy 120, 110 .. 0. */
	int bleed[5][13] = {};
	int phaserRange = 20000, photonRange = 30000, photonDamage = 30;
	/** Spread, in degrees, of a salvo of 1..10 torpedoes. */
	int cone[10] = {};

	static bool loadAll(const Common::String &text, std::vector<ShipClass> &classes);
};

/** A ship line of SCn.MIS (parser 0x4A36A4). */
struct MissionShip {
	Common::String type;
	int x = 0, y = 0, z = 0, yaw = 0, pitch = 0;
	Common::String ai;
	int wave = 1;
};

bool loadCombatMission(const Common::String &text, std::vector<MissionShip> &ships);

/** A font (.MET): the cells of ASCII 32..127 in a .RAS strip, big endian. */
struct MetFont {
	struct Cell {
		int x, y, w, h;
	};
	Cell cells[96] = {};

	bool load(Common::SeekableReadStream &stream);
};

} // End of namespace STG

#endif
