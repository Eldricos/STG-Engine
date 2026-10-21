/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "common/graphics/surface.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace Graphics {

Surface::Surface(Surface &&other) noexcept {
	*this = std::move(other);
}

Surface &Surface::operator=(Surface &&other) noexcept {
	if (this != &other) {
		free();
		w = other.w;
		h = other.h;
		pitch = other.pitch;
		format = other.format;
		_pixels = other._pixels;
		_owned = other._owned;
		other._pixels = nullptr;
		other._owned = false;
		other.w = other.h = 0;
		other.pitch = 0;
	}
	return *this;
}

void Surface::create(int16 width, int16 height, const PixelFormat &fmt) {
	free();
	w = width;
	h = height;
	format = fmt;
	pitch = width * fmt.bytesPerPixel;
	if (width > 0 && height > 0) {
		_pixels = calloc((size_t)height * pitch, 1);
		_owned = true;
	}
}

void Surface::free() {
	if (_owned)
		::free(_pixels);
	_pixels = nullptr;
	_owned = false;
	w = h = 0;
	pitch = 0;
}

void Surface::init(int16 width, int16 height, int32 p, void *pixels, const PixelFormat &fmt) {
	free();
	w = width;
	h = height;
	pitch = p;
	format = fmt;
	_pixels = pixels;
	_owned = false;
}

uint32 Surface::getPixel(int x, int y) const {
	const byte *p = (const byte *)getBasePtr(x, y);
	switch (format.bytesPerPixel) {
	case 1:
		return *p;
	case 2:
		return *(const uint16 *)p;
	case 4:
		return *(const uint32 *)p;
	default:
		return 0;
	}
}

void Surface::setPixel(int x, int y, uint32 color) {
	if (x < 0 || y < 0 || x >= w || y >= h)
		return;
	byte *p = (byte *)getBasePtr(x, y);
	switch (format.bytesPerPixel) {
	case 1:
		*p = (byte)color;
		break;
	case 2:
		*(uint16 *)p = (uint16)color;
		break;
	case 4:
		*(uint32 *)p = color;
		break;
	default:
		break;
	}
}

void Surface::fillRect(Common::Rect r, uint32 color) {
	r.clip(Common::Rect(w, h));
	if (r.isEmpty())
		return;

	for (int y = r.top; y < r.bottom; y++) {
		if (format.bytesPerPixel == 1) {
			memset(getBasePtr(r.left, y), (int)color, r.width());
		} else {
			for (int x = r.left; x < r.right; x++)
				setPixel(x, y, color);
		}
	}
}

void Surface::frameRect(const Common::Rect &r, uint32 color) {
	hLine(r.left, r.top, r.right - 1, color);
	hLine(r.left, r.bottom - 1, r.right - 1, color);
	vLine(r.left, r.top, r.bottom - 1, color);
	vLine(r.right - 1, r.top, r.bottom - 1, color);
}

void Surface::hLine(int x1, int y, int x2, uint32 color) {
	if (x1 > x2)
		std::swap(x1, x2);
	fillRect(Common::Rect(x1, y, x2 + 1, y + 1), color);
}

void Surface::vLine(int x, int y1, int y2, uint32 color) {
	if (y1 > y2)
		std::swap(y1, y2);
	fillRect(Common::Rect(x, y1, x + 1, y2 + 1), color);
}

void Surface::drawLine(int x0, int y0, int x1, int y1, uint32 color) {
	// Bresenham
	int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
	int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
	int err = dx + dy;
	while (true) {
		setPixel(x0, y0, color);
		if (x0 == x1 && y0 == y1)
			break;
		int e2 = 2 * err;
		if (e2 >= dy) {
			err += dy;
			x0 += sx;
		}
		if (e2 <= dx) {
			err += dx;
			y0 += sy;
		}
	}
}

void Surface::fillPolygon(const Common::Point *points, int count, uint32 color) {
	if (count < 3)
		return;

	int minY = points[0].y, maxY = points[0].y;
	for (int i = 1; i < count; i++) {
		minY = std::min<int>(minY, points[i].y);
		maxY = std::max<int>(maxY, points[i].y);
	}
	minY = std::max(minY, 0);
	maxY = std::min<int>(maxY, h - 1);

	std::vector<int> crossings;
	for (int y = minY; y <= maxY; y++) {
		// Sample at the pixel centre; edges include their top end only.
		double sampleY = y + 0.5;
		crossings.clear();
		for (int i = 0; i < count; i++) {
			const Common::Point &a = points[i], &b = points[(i + 1) % count];
			if ((a.y <= sampleY) == (b.y <= sampleY))
				continue;
			double t = (sampleY - a.y) / (double)(b.y - a.y);
			crossings.push_back((int)(a.x + t * (b.x - a.x) + 0.5));
		}
		std::sort(crossings.begin(), crossings.end());
		for (size_t i = 0; i + 1 < crossings.size(); i += 2) {
			if (crossings[i + 1] > crossings[i])
				hLine(crossings[i], y, crossings[i + 1] - 1, color);
		}
	}
}

void Surface::copyRectToSurface(const void *buffer, int srcPitch, int destX, int destY, int width, int height) {
	const byte *src = (const byte *)buffer;

	// Clip against the destination, adjusting the source origin to match.
	if (destX < 0) {
		src -= destX * format.bytesPerPixel;
		width += destX;
		destX = 0;
	}
	if (destY < 0) {
		src -= destY * srcPitch;
		height += destY;
		destY = 0;
	}
	if (destX + width > w)
		width = w - destX;
	if (destY + height > h)
		height = h - destY;
	if (width <= 0 || height <= 0)
		return;

	for (int y = 0; y < height; y++)
		memcpy(getBasePtr(destX, destY + y), src + y * srcPitch, width * format.bytesPerPixel);
}

void Surface::copyRectToSurface(const Surface &src, int destX, int destY, const Common::Rect &subRect) {
	Common::Rect r = subRect;
	r.clip(Common::Rect(src.w, src.h));
	if (r.isEmpty())
		return;
	copyRectToSurface(src.getBasePtr(r.left, r.top), src.pitch, destX, destY, r.width(), r.height());
}

void Surface::transBlitFrom(const Surface &src, int destX, int destY, uint32 transColor) {
	for (int y = 0; y < src.h; y++) {
		int dy = destY + y;
		if (dy < 0 || dy >= h)
			continue;
		for (int x = 0; x < src.w; x++) {
			uint32 color = src.getPixel(x, y);
			if (color != transColor)
				setPixel(destX + x, dy, color);
		}
	}
}

} // End of namespace Graphics
