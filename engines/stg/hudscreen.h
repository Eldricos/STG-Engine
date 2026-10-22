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

#include "common/graphics/palette.h"
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

	/** palette is the level's: the console's bitmaps are drawn in its nearest colours. */
	HudScreen(const ResourceManager &resources, const ResourceRef &level, const ScriptFile &script,
	          const Graphics::Palette &palette);
	~HudScreen();

	/** Whether the console's art was found; without it the viewer keeps its own layout. */
	bool isAvailable() const { return _console != nullptr; }

	LeftMode getLeftMode() const { return _leftMode; }
	void setLeftMode(LeftMode mode) { _leftMode = mode; }

	/** Draw the console (everything but the 3D view) into screen. */
	void draw(Graphics::Surface &screen, Mission &mission, const LevelFile &level, double playerX, double playerY,
	          double playerZ, double yaw, uint32 time);

	/**
	 * A mouse button pressed on the screen; returns false if it missed the
	 * console. Inventory slots act when the button comes up, unless the
	 * item was dragged.
	 */
	bool mouseDown(Mission &mission, int x, int y, bool right);
	void mouseMove(int x, int y);
	/** The left button released; returns false if nothing was pressed on the console. */
	bool mouseUp(Mission &mission, int x, int y);
	/** Draw the item being dragged, over everything else. */
	void drawDrag(Graphics::Surface &screen, Mission &mission);

private:
	/** A bitmap, cut into frames of frameHeight rows (0 for one frame; -n asks for n frames). */
	struct Image {
		/** The bitmap's own indices; keys are compared with these. */
		Graphics::Surface surface;
		/** Its colours in the level's palette (Windows matched a DIB's colours to the screen's). */
		byte remap[256];
		/** The index of the first pixel in the file: the bottom left. */
		byte firstPixel = 0;
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
	enum Key {
		kOpaque = -1,
		/**
		 * See-through where a pixel matches the first one in the file, its
		 * bottom left (0x425A00): the inventory icons' purple, the power
		 * arrow's red, the health bar's magenta.
		 */
		kFirstPixel = -2
	};
	/** Copy frame of image to (x, y), skipping pixels equal to key (an index, or a Key). */
	static void blit(Graphics::Surface &dst, const Image &image, int frame, int x, int y, int key = kOpaque,
	                 const Common::Rect *clip = nullptr);
	void drawLifePanel(Graphics::Surface &screen, Mission &mission);
	void drawMap(Graphics::Surface &screen, Mission &mission, const LevelFile &level, double playerX,
	             double playerY, double playerZ, double yaw, uint32 time);
	void drawPanel(Graphics::Surface &screen, Mission &mission, uint32 time);
	void drawInventory(Graphics::Surface &screen, Mission &mission);
	/** The inventory slot whose hit rectangle holds (x, y), or -1. */
	static int findSlot(int x, int y);

	const ResourceManager &_resources;
	ResourceRef _level;
	const ScriptFile &_script;
	Graphics::Palette _palette;
	std::map<Common::String, std::unique_ptr<Image>> _images;
	const Image *_console = nullptr;
	void drawWeaponDisplay(Graphics::Surface &screen, Mission &mission);

	/** The slot the left button went down on, or -1. */
	int _pressedSlot = -1;
	bool _dragging = false;
	int _pressX = 0, _pressY = 0, _mouseX = 0, _mouseY = 0;
	LeftMode _leftMode = kLeftLife;
	int _mapBand = 1;
	int _mapZoom = 1;
	/** Which markers the map shows: life, energy, trilithium. */
	bool _filters[3] = { true, true, true };
};

} // End of namespace STG

#endif
