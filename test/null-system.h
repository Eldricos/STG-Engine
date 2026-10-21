/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef TEST_NULL_SYSTEM_H
#define TEST_NULL_SYSTEM_H

#include <deque>
#include <memory>

#include "common/audio/mixer.h"
#include "common/graphics/surface.h"
#include "common/system.h"

/**
 * A headless OSystem for tests. It records what the engine does and
 * replays scripted input, which also demonstrates that engines run on any
 * OSystem implementation, not only the SDL one.
 */
class OSystem_Null : public OSystem {
public:
	explicit OSystem_Null(const Common::String &savePath);

	void initSize(uint width, uint height, const Graphics::PixelFormat *format) override;
	int16 getWidth() const override { return _screen.w; }
	int16 getHeight() const override { return _screen.h; }
	Graphics::PixelFormat getScreenFormat() const override { return _screen.format; }
	void setPalette(const byte *colors, uint start, uint num) override;
	void grabPalette(byte *colors, uint start, uint num) const override;
	void copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h) override;
	Graphics::Surface *lockScreen() override { return &_screen; }
	void unlockScreen() override {}
	void updateScreen() override { _frames++; }

	bool showMouse(bool visible) override;
	void warpMouse(int x, int y) override {}
	void setMouseCursor(const void *buf, uint w, uint h, int hotspotX, int hotspotY, uint32 keycolor,
	                    const Graphics::PixelFormat *format) override {
		_cursorSet = true;
	}

	bool pollEvent(Common::Event &event) override;
	uint32 getMillis() const override { return _millis; }
	void delayMillis(uint msecs) override { _millis += msecs; }

	Audio::Mixer *getMixer() override { return &_mixer; }

	Common::String getDefaultSavePath() const override { return _savePath; }
	Common::String getDefaultConfigFileName() const override { return _savePath + "/stg.ini"; }

	void setWindowCaption(const Common::String &caption) override {}
	void logMessage(Common::LogType type, const char *message) override {}
	[[noreturn]] void quit() override;

	/** Deliver this event once the given number of frames have been presented. */
	void scheduleEvent(int frame, const Common::Event &event);

	int getFrameCount() const { return _frames; }
	bool isCursorSet() const { return _cursorSet; }
	bool isMouseVisible() const { return _mouseVisible; }
	const Graphics::Surface &getScreen() const { return _screen; }
	const byte *getPalette() const { return _palette; }

private:
	Common::String _savePath;
	Graphics::Surface _screen;
	byte _palette[256 * 3] = {};
	Audio::Mixer _mixer;
	uint32 _millis = 0;
	int _frames = 0;
	bool _cursorSet = false;
	bool _mouseVisible = false;
	std::deque<std::pair<int, Common::Event>> _script;
};

#endif
