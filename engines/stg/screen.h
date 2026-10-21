/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_SCREEN_H
#define STG_SCREEN_H

#include "common/graphics/palette.h"
#include "common/graphics/surface.h"

class OSystem;

namespace STG {

/**
 * The game's back buffer. Everything is drawn here in 8-bit palettized
 * form and pushed to the OSystem once per frame.
 *
 * 640x480 in 256 colours is assumed from the era and platform; confirm it
 * against the original executable's DirectDraw setup.
 */
class Screen {
public:
	static const int kWidth = 640;
	static const int kHeight = 480;

	explicit Screen(OSystem *system);

	void init();

	Graphics::Surface &getSurface() { return _surface; }
	Graphics::Palette &getPalette() { return _palette; }

	void clear(byte color = 0);

	/** Upload the palette to the backend; call after changing it. */
	void applyPalette();

	/** Draw a surface centred on screen, converting truecolour images to the palette. */
	void drawCentered(const Graphics::Surface &image);

	/** Copy the back buffer to the backend and present it. */
	void update();

private:
	OSystem *_system;
	Graphics::Surface _surface;
	Graphics::Palette _palette;
};

} // End of namespace STG

#endif
