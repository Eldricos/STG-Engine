/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_RENDER3D_H
#define STG_RENDER3D_H

#include <map>
#include <memory>
#include <vector>

#include "common/graphics/surface.h"
#include "engines/stg/level.h"

namespace STG {

/** A viewpoint in world units (not fixed point). */
struct Camera {
	double x = 0;
	double y = 0;
	/** Eye height. */
	double z = 0;
	/** Direction of view in radians; 0 looks along +x, pi/2 along +y. */
	double yaw = 0;
	/**
	 * Looking up or down, as the original does it: a vertical shear that
	 * moves the horizon by pitch * (screen width / 2) pixels.
	 */
	double pitch = 0;
};

/**
 * A software renderer for GAME.LV2 levels, drawing into a CLUT8 surface in
 * the level's palette.
 *
 * It follows the original's camera model: a 90 degree horizontal field of
 * view with the same scale vertically, and looking up and down done by
 * shearing rather than rotating. Rather than the original's sorted spans
 * it uses a depth buffer, which gives the same picture for these levels.
 *
 * Lighting is the original's too: each surface is drawn through one of 8
 * colour tables that blend the palette toward a fog colour (black in every
 * level seen so far). There is no distance fog in the original; the level
 * of each surface comes from the level data and its light group.
 */
class LevelRenderer {
public:
	explicit LevelRenderer(const LevelFile &level);
	~LevelRenderer();

	/** Draw the view from camera; time (ms) drives animated textures. */
	void render(const Camera &camera, Graphics::Surface &dst, uint32 time);

	/**
	 * The highest floor under (x, y) that is no higher than maxHeight.
	 * Returns false when there is none.
	 */
	bool findFloor(double x, double y, double maxHeight, double &height) const;

	/** A starting point: standing in the middle of the largest floor. */
	Camera getStartCamera(double eyeHeight) const;

	/** Turn lighting off to see the textures' own colours. */
	void setLightingEnabled(bool enabled) { _lighting = enabled; }
	bool isLightingEnabled() const { return _lighting; }

	/**
	 * Set a light group's brightness in percent (100, the default, is fully
	 * on), as the scripts do.
	 */
	void setGroupBrightness(int16 group, int percent);

	/** Rebuild the shade tables for a new fog colour. */
	void setFogColor(byte r, byte g, byte b);

	/**
	 * Build the original's shade tables (0x4BFC60): table n maps each
	 * colour to the palette entry nearest to it blended n/8 of the way
	 * toward the fog colour. Table 0 is the identity.
	 */
	static void buildShadeTables(const Graphics::Palette &palette, byte r, byte g, byte b,
	                             byte tables[LevelFile::kLightLevels][256]);

private:
	struct Texture;
	struct Vertex;

	const Texture *getTexture(uint16 id, uint16 frames, uint32 time);
	/** The shade table for a surface, or null to draw it unshaded. */
	const byte *getShade(const LevelFile::LightInfo &light) const;
	void drawSky(const Camera &camera, Graphics::Surface &dst);
	void drawFlats(Graphics::Surface &dst, uint32 time);
	void drawWalls(Graphics::Surface &dst, uint32 time);
	void drawBillboards(Graphics::Surface &dst, uint32 time);

	/** Transform a world point into camera space (x right, y up, z ahead). */
	void toCamera(double x, double y, double z, Vertex &out) const;
	/** Clip a camera-space polygon to the near plane, project and fill it. */
	void drawPolygon(std::vector<Vertex> &poly, const Texture *texture, const byte *shade,
	                 Graphics::Surface &dst);

	const LevelFile &_level;
	std::map<uint32, std::unique_ptr<Texture>> _textures;
	Graphics::Surface _sky;
	bool _hasSky = false;
	byte _shades[LevelFile::kLightLevels][256];
	/** The palette entry nearest the fog colour, shown where there is no sky. */
	byte _fogIndex = 0;
	bool _lighting = true;
	std::map<int16, int> _groupBrightness;
	std::vector<float> _depth;

	// Per-frame view parameters.
	double _cos = 1, _sin = 0;
	double _camX = 0, _camY = 0, _camZ = 0;
	double _centerX = 0, _centerY = 0, _focal = 1;
};

} // End of namespace STG

#endif
