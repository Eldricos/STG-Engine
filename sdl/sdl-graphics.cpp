/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "sdl/sdl-graphics.h"

#include <algorithm>
#include <cstring>

#include "common/debug.h"

SdlGraphicsManager::SdlGraphicsManager() {
	memset(_palette, 0, sizeof(_palette));
	memset(_paletteRGB, 0, sizeof(_paletteRGB));
}

SdlGraphicsManager::~SdlGraphicsManager() {
	if (_texture)
		SDL_DestroyTexture(_texture);
	if (_renderer)
		SDL_DestroyRenderer(_renderer);
	if (_window)
		SDL_DestroyWindow(_window);
}

void SdlGraphicsManager::setWindowCaption(const Common::String &caption) {
	_caption = caption;
	if (_window)
		SDL_SetWindowTitle(_window, caption.c_str());
}

void SdlGraphicsManager::createWindow(uint width, uint height) {
	// Start at the largest whole multiple of the game size that fits the desktop.
	int scale = 1;
	SDL_Rect usable;
	if (SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable)) {
		while ((uint)usable.w >= width * (scale + 1) && (uint)usable.h >= height * (scale + 1) && scale < 4)
			scale++;
	}

	SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
	if (_fullscreen)
		flags |= SDL_WINDOW_FULLSCREEN;

	_window = SDL_CreateWindow(_caption.c_str(), width * scale, height * scale, flags);
	if (!_window)
		error("Could not create window: %s", SDL_GetError());

	_renderer = SDL_CreateRenderer(_window, nullptr);
	if (!_renderer)
		error("Could not create renderer: %s", SDL_GetError());
	SDL_SetRenderVSync(_renderer, 1);

	debug(1, "Using SDL renderer '%s'", SDL_GetRendererName(_renderer));
}

void SdlGraphicsManager::initSize(uint width, uint height, const Graphics::PixelFormat &format) {
	if (!_window)
		createWindow(width, height);

	if (_texture) {
		SDL_DestroyTexture(_texture);
		_texture = nullptr;
	}

	_screen.create((int16)width, (int16)height, format);
	_frame.assign((size_t)width * height, 0);

	_texture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
	if (!_texture)
		error("Could not create screen texture: %s", SDL_GetError());

	updatePresentation();
	debug(1, "Game screen %ux%u %s", width, height, format.toString().c_str());
}

void SdlGraphicsManager::updatePresentation() {
	if (!_renderer || !_texture)
		return;
	SDL_SetTextureScaleMode(_texture, _filtering ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
	SDL_SetRenderLogicalPresentation(_renderer, _screen.w, _screen.h,
	                                 _integerScaling ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
	                                                 : SDL_LOGICAL_PRESENTATION_LETTERBOX);
}

void SdlGraphicsManager::setPalette(const byte *colors, uint start, uint num) {
	if (start >= 256)
		return;
	num = std::min(num, 256 - start);
	memcpy(_paletteRGB + start * 3, colors, num * 3);
	for (uint i = 0; i < num; i++) {
		const byte *c = colors + i * 3;
		_palette[start + i] = 0xFF000000 | ((uint32)c[0] << 16) | ((uint32)c[1] << 8) | c[2];
	}
}

void SdlGraphicsManager::grabPalette(byte *colors, uint start, uint num) const {
	if (start >= 256)
		return;
	num = std::min(num, 256 - start);
	memcpy(colors, _paletteRGB + start * 3, num * 3);
}

void SdlGraphicsManager::copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h) {
	_screen.copyRectToSurface(buf, pitch, x, y, w, h);
}

Graphics::Surface *SdlGraphicsManager::lockScreen() {
	return &_screen;
}

uint32 SdlGraphicsManager::colorToXRGB(uint32 color, const Graphics::PixelFormat &format) const {
	if (format.isCLUT8())
		return _palette[color & 0xFF];
	byte r, g, b;
	format.colorToRGB(color, r, g, b);
	return 0xFF000000 | ((uint32)r << 16) | ((uint32)g << 8) | b;
}

void SdlGraphicsManager::drawCursor() {
	if (!_cursorVisible || _cursor.empty())
		return;

	int x0 = _mouseX - _cursorHotspotX;
	int y0 = _mouseY - _cursorHotspotY;
	for (int y = 0; y < _cursor.h; y++) {
		int dy = y0 + y;
		if (dy < 0 || dy >= _screen.h)
			continue;
		for (int x = 0; x < _cursor.w; x++) {
			int dx = x0 + x;
			if (dx < 0 || dx >= _screen.w)
				continue;
			uint32 color = _cursor.getPixel(x, y);
			if (color != _cursorKeyColor)
				_frame[dy * _screen.w + dx] = colorToXRGB(color, _cursor.format);
		}
	}
}

void SdlGraphicsManager::updateScreen() {
	if (!_texture)
		return;

	const int w = _screen.w, h = _screen.h;
	if (_screen.format.isCLUT8()) {
		for (int y = 0; y < h; y++) {
			const byte *src = (const byte *)_screen.getBasePtr(0, y);
			uint32 *dst = &_frame[y * w];
			for (int x = 0; x < w; x++)
				dst[x] = _palette[src[x]];
		}
	} else {
		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++)
				_frame[y * w + x] = colorToXRGB(_screen.getPixel(x, y), _screen.format);
		}
	}

	drawCursor();

	SDL_UpdateTexture(_texture, nullptr, _frame.data(), w * 4);
	SDL_SetRenderDrawColor(_renderer, 0, 0, 0, 255);
	SDL_RenderClear(_renderer);
	SDL_RenderTexture(_renderer, _texture, nullptr, nullptr);
	SDL_RenderPresent(_renderer);
}

bool SdlGraphicsManager::showMouse(bool visible) {
	bool last = _cursorVisible;
	_cursorVisible = visible;
	// The cursor is drawn in software at game resolution, so hide the host one.
	if (_window)
		SDL_HideCursor();
	return last;
}

void SdlGraphicsManager::warpMouse(int x, int y) {
	setMousePos(x, y);
	if (!_renderer)
		return;
	float wx, wy;
	if (SDL_RenderCoordinatesToWindow(_renderer, (float)x, (float)y, &wx, &wy))
		SDL_WarpMouseInWindow(_window, wx, wy);
}

void SdlGraphicsManager::setMousePos(int x, int y) {
	_mouseX = x;
	_mouseY = y;
}

void SdlGraphicsManager::setMouseCursor(const void *buf, uint w, uint h, int hotspotX, int hotspotY, uint32 keycolor,
                                        const Graphics::PixelFormat &format) {
	_cursor.create((int16)w, (int16)h, format);
	_cursor.copyRectToSurface(buf, w * format.bytesPerPixel, 0, 0, w, h);
	_cursorHotspotX = hotspotX;
	_cursorHotspotY = hotspotY;
	_cursorKeyColor = keycolor;
}

void SdlGraphicsManager::setFullscreen(bool enable) {
	_fullscreen = enable;
	if (_window)
		SDL_SetWindowFullscreen(_window, enable);
}

void SdlGraphicsManager::setFiltering(bool enable) {
	_filtering = enable;
	updatePresentation();
}

void SdlGraphicsManager::setIntegerScaling(bool enable) {
	_integerScaling = enable;
	updatePresentation();
}

void SdlGraphicsManager::convertEventCoordinates(SDL_Event &event) const {
	if (!_renderer)
		return;
	SDL_ConvertEventToRenderCoordinates(_renderer, &event);

	auto clamp = [](float v, int max) { return std::clamp((int)v, 0, std::max(0, max - 1)); };
	switch (event.type) {
	case SDL_EVENT_MOUSE_MOTION:
		event.motion.x = (float)clamp(event.motion.x, _screen.w);
		event.motion.y = (float)clamp(event.motion.y, _screen.h);
		break;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP:
		event.button.x = (float)clamp(event.button.x, _screen.w);
		event.button.y = (float)clamp(event.button.y, _screen.h);
		break;
	default:
		break;
	}
}
