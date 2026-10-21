/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_GRAPHICS_SURFACE_H
#define COMMON_GRAPHICS_SURFACE_H

#include "common/graphics/pixelformat.h"
#include "common/rect.h"
#include "common/types.h"

namespace Graphics {

/**
 * A 2D pixel buffer. Engines draw into surfaces and hand them to
 * OSystem::copyRectToScreen(); the backend converts to whatever the host
 * actually displays.
 */
class Surface {
public:
	int16 w = 0;
	int16 h = 0;
	int32 pitch = 0;
	PixelFormat format;

	Surface() {}
	~Surface() { free(); }

	Surface(const Surface &) = delete;
	Surface &operator=(const Surface &) = delete;
	Surface(Surface &&other) noexcept;
	Surface &operator=(Surface &&other) noexcept;

	void create(int16 width, int16 height, const PixelFormat &fmt);
	void free();

	/** Make this surface point at memory it does not own. */
	void init(int16 width, int16 height, int32 pitch, void *pixels, const PixelFormat &fmt);

	bool empty() const { return _pixels == nullptr; }

	void *getPixels() { return _pixels; }
	const void *getPixels() const { return _pixels; }

	void *getBasePtr(int x, int y) {
		return (byte *)_pixels + y * pitch + x * format.bytesPerPixel;
	}
	const void *getBasePtr(int x, int y) const {
		return (const byte *)_pixels + y * pitch + x * format.bytesPerPixel;
	}

	uint32 getPixel(int x, int y) const;
	void setPixel(int x, int y, uint32 color);

	void fillRect(Common::Rect r, uint32 color);
	void frameRect(const Common::Rect &r, uint32 color);
	void hLine(int x1, int y, int x2, uint32 color);
	void vLine(int x, int y1, int y2, uint32 color);
	void drawLine(int x0, int y0, int x1, int y1, uint32 color);

	/** Copy a block of pixels in this surface's format, clipping to its bounds. */
	void copyRectToSurface(const void *buffer, int srcPitch, int destX, int destY, int width, int height);
	void copyRectToSurface(const Surface &src, int destX, int destY, const Common::Rect &subRect);

	/** As copyRectToSurface, but pixels equal to transColor are skipped. */
	void transBlitFrom(const Surface &src, int destX, int destY, uint32 transColor);

private:
	void *_pixels = nullptr;
	bool _owned = false;
};

} // End of namespace Graphics

#endif
