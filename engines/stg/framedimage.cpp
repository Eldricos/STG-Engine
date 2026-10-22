/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/framedimage.h"

#include <algorithm>

#include "common/image/bmp.h"

namespace STG {

void FramedImage::set(const ::Image::BitmapDecoder &decoder, const Graphics::Palette &palette, int height) {
	const Graphics::Surface &s = decoder.getSurface();
	surface.create(s.w, s.h, Graphics::PixelFormat::createFormatCLUT8());
	surface.copyRectToSurface(s, 0, 0, Common::Rect(s.w, s.h));
	firstPixel = *(const byte *)s.getBasePtr(0, s.h - 1);
	const Graphics::Palette &own = decoder.getPalette();
	for (int i = 0; i < 256; i++) {
		byte r, g, b;
		own.get(i, r, g, b);
		remap[i] = palette.findBestColor(r, g, b);
	}
	frameHeight = height < 0 ? s.h / -height : height;
}

int FramedImage::getFrameCount() const {
	return frameHeight > 0 ? std::max(1, (int)surface.h / frameHeight) : 1;
}

void FramedImage::draw(Graphics::Surface &dst, int frame, int x, int y, int key, const Common::Rect *clip) const {
	if (key == kFirstPixel)
		key = firstPixel;
	int height = getHeight();
	// Frames count up from the bottom of the strip: 0x40390E takes frame n
	// at (count - n - 1) frames into rows already turned top down.
	frame = getFrameCount() - 1 - std::max(0, std::min(frame, getFrameCount() - 1));
	Common::Rect area(dst.w, dst.h);
	if (clip)
		area.clip(*clip);
	for (int row = 0; row < height; row++) {
		int sy = y + row;
		if (sy < area.top || sy >= area.bottom)
			continue;
		const byte *src = (const byte *)surface.getBasePtr(0, frame * height + row);
		byte *out = (byte *)dst.getBasePtr(0, sy);
		for (int col = 0; col < surface.w; col++) {
			int sx = x + col;
			if (sx < area.left || sx >= area.right || src[col] == key)
				continue;
			out[sx] = remap[src[col]];
		}
	}
}

} // End of namespace STG
