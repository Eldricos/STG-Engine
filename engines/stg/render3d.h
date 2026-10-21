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

/** A sprite to draw this frame, such as an actor. */
struct SpriteDraw {
	/** Centre of the bottom edge, in world units. */
	double x, y, bottom;
	double halfWidth, height;
	/** Upright frame; pixels equal to 0xFF are transparent. */
	const Graphics::Surface *surface;
	bool mirrored;
	LevelFile::LightInfo light;
	bool fullbright;
	/** Drawn see-through; see LevelFile::kFlagTranslucent. */
	bool translucent = false;
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
	 * A starting point until the scripts can place the player: the middle
	 * of the largest floor at the height with the most floor area, not
	 * counting each height's largest piece (usually scenery around the play
	 * area).
	 */
	Camera getStartCamera(double eyeHeight) const;

	/**
	 * Draw only walls, flats and billboards in these zones, as the original
	 * does (see Walker::getActiveZones()). All zones are drawn by default.
	 */
	void setVisibleZones(uint64 zones) { _zones = zones; }

	/** Without the sky, the background is the fog colour. */
	void setSkyEnabled(bool enabled) { _skyEnabled = enabled; }

	/** Sprites to draw in the next frames; the vector must stay alive. */
	void setSprites(const std::vector<SpriteDraw> *sprites) { _sprites = sprites; }

	/** Turn lighting off to see the textures' own colours. */
	void setLightingEnabled(bool enabled) { _lighting = enabled; }
	bool isLightingEnabled() const { return _lighting; }

	/**
	 * Set a light group's brightness in percent (100, the default, is fully
	 * on), as the scripts do.
	 */
	void setGroupBrightness(int16 group, int percent);

	/** The first palette entry that see-through things blend; the rest up to 0xFE do too. */
	static const int kBlendFirst = 0xF0;
	static const int kBlendColors = 15;

	/**
	 * The original's blend table (0x4BD620): for each of the palette
	 * entries 0xF0-0xFE drawn over each background colour, the entry
	 * nearest their average.
	 */
	static void buildBlendTable(const Graphics::Palette &palette, byte table[kBlendColors][256]);

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

	const Texture *getTexture(uint16 id, uint16 frames, uint16 speed, uint32 time);
	/** The shade table for a surface, or null to draw it unshaded. */
	const byte *getShade(const LevelFile::LightInfo &light) const;
	void drawSky(const Camera &camera, Graphics::Surface &dst);
	void drawFlats(Graphics::Surface &dst, uint32 time);
	void drawWalls(Graphics::Surface &dst, uint32 time);
	void drawBillboards(Graphics::Surface &dst, uint32 time);
	void drawSprites(Graphics::Surface &dst);
	/** A sprite frame in the renderer's column-major layout. */
	const Texture *getSpriteTexture(const Graphics::Surface *surface);

	/** Transform a world point into camera space (x right, y up, z ahead). */
	void toCamera(double x, double y, double z, Vertex &out) const;
	/** How a texture continues past its edges. */
	enum Wrap {
		kWrapTile,     ///< repeats both ways
		kWrapOnceDown, ///< repeats across; above and below its one copy there is nothing
		kWrapStretch   ///< its edges stretch, for sprites
	};

	/** Clip a camera-space polygon to the near plane, project and fill it. */
	void drawPolygon(std::vector<Vertex> &poly, const Texture *texture, const byte *shade,
	                 Graphics::Surface &dst, Wrap wrap = kWrapTile, bool translucent = false);
	/** Draw now, or after everything solid if translucent. */
	void queuePolygon(std::vector<Vertex> &poly, const Texture *texture, const byte *shade,
	                  Graphics::Surface &dst, Wrap wrap, bool translucent);
	/** The see-through polygons queued this frame, far to near. */
	void drawTranslucent(Graphics::Surface &dst);

	struct Translucent {
		std::vector<Vertex> poly;
		const Texture *texture;
		const byte *shade;
		Wrap wrap;
		double depth;
	};
	std::vector<Translucent> _translucent;

	const LevelFile &_level;
	std::map<uint32, std::unique_ptr<Texture>> _textures;
	std::map<const Graphics::Surface *, std::unique_ptr<Texture>> _spriteTextures;
	const std::vector<SpriteDraw> *_sprites = nullptr;
	Graphics::Surface _sky;
	bool _hasSky = false;
	bool _skyEnabled = true;
	byte _shades[LevelFile::kLightLevels][256];
	byte _blend[kBlendColors][256];
	/** The palette entry nearest the fog colour, shown where there is no sky. */
	byte _fogIndex = 0;
	bool _lighting = true;
	uint64 _zones = ~(uint64)0;
	std::map<int16, int> _groupBrightness;
	std::vector<float> _depth;

	// Per-frame view parameters.
	double _cos = 1, _sin = 0;
	double _camX = 0, _camY = 0, _camZ = 0;
	double _centerX = 0, _centerY = 0, _focal = 1;
};

} // End of namespace STG

#endif
