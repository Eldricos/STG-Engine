/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SDL_SDL_GRAPHICS_H
#define SDL_SDL_GRAPHICS_H

#include <SDL3/SDL.h>

#include <vector>

#include "common/graphics/surface.h"
#include "common/str.h"

/**
 * Presents the game screen with SDL_Renderer.
 *
 * The engine draws into a surface in the game's own format (usually CLUT8);
 * on every update the surface is converted to XRGB8888 with the current
 * palette, the software cursor is composited on top, and the result is
 * uploaded to a streaming texture which the renderer scales to the window
 * with letterboxing.
 */
class SdlGraphicsManager {
public:
	SdlGraphicsManager();
	~SdlGraphicsManager();

	void setWindowCaption(const Common::String &caption);

	void initSize(uint width, uint height, const Graphics::PixelFormat &format);
	int16 getWidth() const { return _screen.w; }
	int16 getHeight() const { return _screen.h; }
	Graphics::PixelFormat getScreenFormat() const { return _screen.format; }

	void setPalette(const byte *colors, uint start, uint num);
	void grabPalette(byte *colors, uint start, uint num) const;

	void copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h);
	Graphics::Surface *lockScreen();
	void unlockScreen() {}
	void updateScreen();

	bool showMouse(bool visible);
	void warpMouse(int x, int y);
	void setMouseCursor(const void *buf, uint w, uint h, int hotspotX, int hotspotY, uint32 keycolor,
	                    const Graphics::PixelFormat &format);
	void setMousePos(int x, int y);

	void setFullscreen(bool enable);
	bool isFullscreen() const { return _fullscreen; }
	void setFiltering(bool enable);
	bool isFiltering() const { return _filtering; }
	void setIntegerScaling(bool enable);
	bool isIntegerScaling() const { return _integerScaling; }

	SDL_Window *getWindow() const { return _window; }

	/**
	 * Convert an SDL event's coordinates from window space to game screen
	 * space, clamping them to the screen.
	 */
	void convertEventCoordinates(SDL_Event &event) const;

private:
	void createWindow(uint width, uint height);
	void updatePresentation();
	uint32 colorToXRGB(uint32 color, const Graphics::PixelFormat &format) const;
	void drawCursor();

	SDL_Window *_window = nullptr;
	SDL_Renderer *_renderer = nullptr;
	SDL_Texture *_texture = nullptr;
	Common::String _caption = "STG-Engine";

	Graphics::Surface _screen;
	uint32 _palette[256];
	byte _paletteRGB[256 * 3];
	std::vector<uint32> _frame;

	Graphics::Surface _cursor;
	int _cursorHotspotX = 0, _cursorHotspotY = 0;
	uint32 _cursorKeyColor = 0;
	bool _cursorVisible = false;
	int _mouseX = 0, _mouseY = 0;

	bool _fullscreen = false;
	bool _filtering = false;
	bool _integerScaling = false;
};

#endif
