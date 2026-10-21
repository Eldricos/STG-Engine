/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "test/null-system.h"

#include <cstdlib>
#include <cstring>

OSystem_Null::OSystem_Null(const Common::String &savePath) : _savePath(savePath), _mixer(22050) {
	_mixer.setReady(true);
}

void OSystem_Null::initSize(uint width, uint height, const Graphics::PixelFormat *format) {
	_screen.create((int16)width, (int16)height, format ? *format : Graphics::PixelFormat::createFormatCLUT8());
}

void OSystem_Null::setPalette(const byte *colors, uint start, uint num) {
	memcpy(_palette + start * 3, colors, num * 3);
}

void OSystem_Null::grabPalette(byte *colors, uint start, uint num) const {
	memcpy(colors, _palette + start * 3, num * 3);
}

void OSystem_Null::copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h) {
	_screen.copyRectToSurface(buf, pitch, x, y, w, h);
}

bool OSystem_Null::showMouse(bool visible) {
	bool last = _mouseVisible;
	_mouseVisible = visible;
	return last;
}

bool OSystem_Null::pollEvent(Common::Event &event) {
	// Let the mixer run as if an audio device were pulling samples.
	int16 samples[256];
	_mixer.mixCallback(samples, 128);

	if (_script.empty() || _script.front().first > _frames)
		return false;
	event = _script.front().second;
	_script.pop_front();
	return true;
}

void OSystem_Null::quit() {
	exit(0);
}

void OSystem_Null::scheduleEvent(int frame, const Common::Event &event) {
	_script.push_back({ frame, event });
}
