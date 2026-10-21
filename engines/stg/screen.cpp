/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/screen.h"

#include "common/system.h"

namespace STG {

Screen::Screen(OSystem *system) : _system(system) {
}

void Screen::init() {
	_system->initSize(kWidth, kHeight);
	_surface.create(kWidth, kHeight, Graphics::PixelFormat::createFormatCLUT8());
}

void Screen::clear(byte color) {
	_surface.fillRect(Common::Rect(kWidth, kHeight), color);
}

void Screen::applyPalette() {
	_system->setPalette(_palette.data(), 0, Graphics::Palette::kSize);
}

void Screen::drawCentered(const Graphics::Surface &image) {
	int x = (kWidth - image.w) / 2;
	int y = (kHeight - image.h) / 2;

	if (image.format.isCLUT8()) {
		_surface.copyRectToSurface(image, x, y, Common::Rect(image.w, image.h));
		return;
	}

	// Truecolour: map each pixel onto the current palette.
	for (int sy = 0; sy < image.h; sy++) {
		for (int sx = 0; sx < image.w; sx++) {
			byte r, g, b;
			image.format.colorToRGB(image.getPixel(sx, sy), r, g, b);
			_surface.setPixel(x + sx, y + sy, _palette.findBestColor(r, g, b));
		}
	}
}

void Screen::update() {
	_system->copyRectToScreen(_surface.getPixels(), _surface.pitch, 0, 0, kWidth, kHeight);
	_system->updateScreen();
}

} // End of namespace STG
