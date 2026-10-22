/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/rgbimage.h"

#include <algorithm>

#include "common/image/bmp.h"
#include "engines/stg/overlay.h"

namespace STG {

namespace {

uint32 toRgb(const Graphics::Palette &palette, byte index) {
	byte r, g, b;
	palette.get(index, r, g, b);
	return 0xFF000000u | (uint32)r << 16 | (uint32)g << 8 | b;
}

} // End of anonymous namespace

bool RgbImage::load(const ::Image::BitmapDecoder &decoder, int frames, int key) {
	const Graphics::Surface &s = decoder.getSurface();
	if (!s.format.isCLUT8() || s.w <= 0 || s.h <= 0)
		return false;
	const Graphics::Palette &palette = decoder.getPalette();
	_w = s.w;
	_h = s.h;
	_frames = std::max(1, frames);
	_frameHeight = std::max(1, _h / _frames);
	_fromBottom = true;
	_pixels.resize((size_t)_w * _h);
	for (int y = 0; y < _h; y++) {
		const byte *src = (const byte *)s.getBasePtr(0, y);
		for (int x = 0; x < _w; x++)
			_pixels[(size_t)y * _w + x] = src[x] == key ? 0 : toRgb(palette, src[x]);
	}
	return true;
}

void RgbImage::load(const Overlay &overlay, const Graphics::Palette &palette, int frameHeight) {
	const Graphics::Surface &s = overlay.getSurface();
	_w = s.w;
	_h = s.h;
	_frameHeight = std::max(1, std::min(frameHeight, _h));
	_frames = std::max(1, (_h + _frameHeight - 1) / _frameHeight);
	_fromBottom = false;
	_pixels.assign((size_t)_w * _frames * _frameHeight, 0);
	// Each frame's rows are stored bottom up.
	for (int f = 0; f < _frames; f++) {
		int top = f * _frameHeight, height = std::min(_frameHeight, _h - top);
		for (int y = 0; y < height; y++) {
			int srcY = top + height - 1 - y;
			const byte *src = (const byte *)s.getBasePtr(0, srcY);
			uint32 *out = &_pixels[((size_t)top + y) * _w];
			for (int x = 0; x < _w; x++)
				out[x] = overlay.isOpaque(x, srcY) ? toRgb(palette, src[x]) : 0;
		}
	}
	_h = _frames * _frameHeight;
}

const uint32 *RgbImage::row(int frame, int y) const {
	frame = std::max(0, std::min(frame, _frames - 1));
	if (_fromBottom)
		frame = _frames - 1 - frame;
	return &_pixels[((size_t)frame * _frameHeight + y) * _w];
}

uint32 RgbImage::getPixel(int frame, int x, int y) const {
	if (!isLoaded() || x < 0 || y < 0 || x >= _w || y >= _frameHeight)
		return 0;
	return row(frame, y)[x];
}

void RgbImage::draw(Graphics::Surface &dst, int frame, int x, int y, const Common::Rect *clip) const {
	drawScaled(dst, frame, x, y, _w, _frameHeight, clip);
}

void RgbImage::drawScaled(Graphics::Surface &dst, int frame, int x, int y, int w, int h,
                          const Common::Rect *clip) const {
	if (!isLoaded() || w <= 0 || h <= 0)
		return;
	Common::Rect area(dst.w, dst.h);
	if (clip)
		area.clip(*clip);
	int x0 = std::max<int>(x, area.left), x1 = std::min<int>(x + w, area.right);
	int y0 = std::max<int>(y, area.top), y1 = std::min<int>(y + h, area.bottom);
	for (int dy = y0; dy < y1; dy++) {
		const uint32 *src = row(frame, (dy - y) * _frameHeight / h);
		uint32 *out = (uint32 *)dst.getBasePtr(0, dy);
		for (int dx = x0; dx < x1; dx++) {
			uint32 c = src[(dx - x) * _w / w];
			if (c >> 24)
				out[dx] = c;
		}
	}
}

} // End of namespace STG
