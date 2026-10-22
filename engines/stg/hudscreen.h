/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_HUDSCREEN_H
#define STG_HUDSCREEN_H

#include <map>
#include <memory>

#include "common/graphics/surface.h"
#include "common/rect.h"
#include "engines/stg/resources.h"

namespace STG {

class LevelFile;
class Mission;
class ScriptFile;

/**
 * The away-team screen: the 3D view in the top 640 x 252 and the console
 * below it, drawn from the game's bitmaps as the original lays it out.
 *
 * - Left, the life / map / object panel (l-m-m-o.bmp, tabs B, H, O): the
 *   body picture and health bar, the automap, or the mission's OBJECT.BMP.
 * - Middle, the weapon display, and the close-up viewer for panels the
 *   scripts open (the mission's AUXDEF.BMP when none is).
 * - Right, the inventory: 12 slots of icons from the mission's INVENT.BMP.
 * - A line of text along the bottom.
 *
 * Plain names (actionx.bmp, map\dot-romu.bmp) come from COMMON.DAT, the
 * mission's own from its folder; script images are its resource names, a
 * leading * meaning COMMON.DAT.
 */
class HudScreen {
public:
	static const Common::Rect kView;
	static const Common::Rect kTextLine;

	enum LeftMode {
		kLeftOff,
		kLeftLife,
		kLeftMap,
		kLeftObject
	};

	HudScreen(const ResourceManager &resources, const ResourceRef &level, const ScriptFile &script);
	~HudScreen();

	/** Whether the console's art was found; without it the viewer keeps its own layout. */
	bool isAvailable() const { return _console != nullptr; }

	LeftMode getLeftMode() const { return _leftMode; }
	void setLeftMode(LeftMode mode) { _leftMode = mode; }

	/** Draw the console (everything but the 3D view) into screen. */
	void draw(Graphics::Surface &screen, Mission &mission, const LevelFile &level, double playerX, double playerY,
	          double playerZ, double yaw, uint32 time);

	/** A mouse click on the console; returns false if it missed everything. */
	bool click(Mission &mission, int x, int y, bool right);

private:
	/** A bitmap, cut into frames of frameHeight rows (0 for one frame; -n asks for n frames). */
	struct Image {
		Graphics::Surface surface;
		int frameHeight = 0;
		int getFrameCount() const;
	};
	enum Scope {
		kGlobal,  ///< COMMON.DAT
		kMission, ///< beside GAME.LV2
		kScript   ///< a script resource index
	};
	const Image *load(const Common::String &name, Scope scope, int frameHeight = 0);
	const Image *loadScriptImage(int16 index, int frameHeight);
	/** Copy frame of image to (x, y), skipping keyed pixels unless key < 0. */
	static void blit(Graphics::Surface &dst, const Image &image, int frame, int x, int y, int key = -1,
	                 const Common::Rect *clip = nullptr);
	void drawLifePanel(Graphics::Surface &screen, Mission &mission);
	void drawMap(Graphics::Surface &screen, Mission &mission, const LevelFile &level, double playerX,
	             double playerY, double playerZ, double yaw);
	void drawPanel(Graphics::Surface &screen, Mission &mission, uint32 time);
	void drawInventory(Graphics::Surface &screen, Mission &mission);

	const ResourceManager &_resources;
	ResourceRef _level;
	const ScriptFile &_script;
	std::map<Common::String, std::unique_ptr<Image>> _images;
	const Image *_console = nullptr;
	LeftMode _leftMode = kLeftLife;
	int _mapBand = 1;
	double _mapScale = 1.0;
};

} // End of namespace STG

#endif
