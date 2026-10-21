/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SDL_SDL_SYSTEM_H
#define SDL_SDL_SYSTEM_H

#include <memory>

#include "common/system.h"

class SdlEventSource;
class SdlGraphicsManager;
class SdlMixerManager;

/** OSystem implementation on top of SDL3, for every desktop platform SDL supports. */
class OSystem_SDL : public OSystem {
public:
	OSystem_SDL();
	~OSystem_SDL() override;

	void initBackend() override;

	bool hasFeature(Feature f) const override;
	void setFeatureState(Feature f, bool enable) override;
	bool getFeatureState(Feature f) const override;

	std::vector<Graphics::PixelFormat> getSupportedFormats() const override;
	void initSize(uint width, uint height, const Graphics::PixelFormat *format) override;
	int16 getWidth() const override;
	int16 getHeight() const override;
	Graphics::PixelFormat getScreenFormat() const override;
	void setPalette(const byte *colors, uint start, uint num) override;
	void grabPalette(byte *colors, uint start, uint num) const override;
	void copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h) override;
	Graphics::Surface *lockScreen() override;
	void unlockScreen() override;
	void updateScreen() override;

	bool showMouse(bool visible) override;
	void warpMouse(int x, int y) override;
	void setMouseCursor(const void *buf, uint w, uint h, int hotspotX, int hotspotY, uint32 keycolor,
	                    const Graphics::PixelFormat *format) override;

	bool pollEvent(Common::Event &event) override;
	uint32 getMillis() const override;
	void delayMillis(uint msecs) override;

	Audio::Mixer *getMixer() override;

	Common::String getDefaultSavePath() const override;
	Common::String getDefaultConfigFileName() const override;

	void setWindowCaption(const Common::String &caption) override;
	void logMessage(Common::LogType type, const char *message) override;
	void messageBox(Common::LogType type, const char *message) override;
	[[noreturn]] void quit() override;

private:
	Common::String getUserDataPath() const;
	void shutdown();

	bool _initialized = false;
	std::unique_ptr<SdlGraphicsManager> _graphics;
	std::unique_ptr<SdlEventSource> _events;
	std::unique_ptr<SdlMixerManager> _mixer;
	mutable Common::String _userDataPath;
};

#endif
