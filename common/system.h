/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_SYSTEM_H
#define COMMON_SYSTEM_H

#include <memory>
#include <vector>

#include "common/debug.h"
#include "common/events.h"
#include "common/graphics/pixelformat.h"
#include "common/savefile.h"
#include "common/str.h"
#include "common/types.h"

namespace Audio {
class Mixer;
}

namespace Graphics {
class Surface;
}

/**
 * The interface between engines and the host platform.
 *
 * This is the only route by which engine code reaches the display, input,
 * audio output and clock. A backend (see the sdl/ directory) implements it;
 * engines only ever include headers from common/ and their own directory.
 */
class OSystem {
public:
	enum Feature {
		/** Toggle between windowed and fullscreen display. */
		kFeatureFullscreenMode,
		/** Stretch 320x200-style modes to a 4:3 aspect ratio. */
		kFeatureAspectRatioCorrection,
		/** Bilinear filtering when scaling instead of nearest neighbour. */
		kFeatureFilteringMode,
		/** Scale the game only by whole multiples. */
		kFeatureIntegerScaling,
		/** Engine may use a native message box for fatal errors. */
		kFeatureMessageBox
	};

	OSystem();
	virtual ~OSystem();

	OSystem(const OSystem &) = delete;
	OSystem &operator=(const OSystem &) = delete;

	/**
	 * Called once before any other method. Implementations must call the
	 * base version, which creates the event and save file managers.
	 */
	virtual void initBackend();

	virtual bool hasFeature(Feature f) const { return false; }
	virtual void setFeatureState(Feature f, bool enable) {}
	virtual bool getFeatureState(Feature f) const { return false; }

	/** @name Graphics */
	/** @{ */

	/** Pixel formats the backend can display, best first. CLUT8 is always supported. */
	virtual std::vector<Graphics::PixelFormat> getSupportedFormats() const;

	/**
	 * Set the size and format of the game screen. A null format means CLUT8.
	 * The window is scaled to fit; the engine always works in game pixels.
	 */
	virtual void initSize(uint width, uint height, const Graphics::PixelFormat *format = nullptr) = 0;

	virtual int16 getWidth() const = 0;
	virtual int16 getHeight() const = 0;
	virtual Graphics::PixelFormat getScreenFormat() const = 0;

	/** Set colours of the CLUT8 palette; colors holds num RGB triplets. */
	virtual void setPalette(const byte *colors, uint start, uint num) = 0;
	virtual void grabPalette(byte *colors, uint start, uint num) const = 0;

	/** Copy a block of pixels, in the screen format, onto the game screen. */
	virtual void copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h) = 0;

	/**
	 * Direct access to the game screen. Must be followed by unlockScreen()
	 * before the next updateScreen().
	 */
	virtual Graphics::Surface *lockScreen() = 0;
	virtual void unlockScreen() = 0;

	virtual void fillScreen(uint32 color);

	/** Present the current game screen. Usually called once per frame. */
	virtual void updateScreen() = 0;

	/** @} */

	/** @name Mouse */
	/** @{ */

	/** Returns the previous visibility. */
	virtual bool showMouse(bool visible) = 0;
	virtual void warpMouse(int x, int y) = 0;

	/**
	 * Set the mouse cursor image. Pixels equal to keycolor are transparent.
	 * A null format means CLUT8, drawn using the screen palette.
	 */
	virtual void setMouseCursor(const void *buf, uint w, uint h, int hotspotX, int hotspotY, uint32 keycolor,
	                            const Graphics::PixelFormat *format = nullptr) = 0;

	/** @} */

	/** @name Events and time */
	/** @{ */

	/** Fetch the next host event. Engines should use getEventManager() instead. */
	virtual bool pollEvent(Common::Event &event) = 0;

	Common::EventManager *getEventManager() { return _eventManager.get(); }

	/** Milliseconds since an arbitrary fixed point. */
	virtual uint32 getMillis() const = 0;
	virtual void delayMillis(uint msecs) = 0;

	/** @} */

	/** The software mixer. Backends feed its output to the audio device. */
	virtual Audio::Mixer *getMixer() = 0;

	/** @name Paths and storage */
	/** @{ */

	Common::SaveFileManager *getSavefileManager() { return _savefileManager.get(); }

	/** Per-user directory for saved games. */
	virtual Common::String getDefaultSavePath() const = 0;

	/** Full path of the configuration file. */
	virtual Common::String getDefaultConfigFileName() const = 0;

	/** @} */

	/** @name Miscellaneous */
	/** @{ */

	virtual void setWindowCaption(const Common::String &caption) = 0;

	/** Write a log message to wherever the platform keeps them. */
	virtual void logMessage(Common::LogType type, const char *message);

	/** Show a native message box, if supported. */
	virtual void messageBox(Common::LogType type, const char *message) {}

	/** Shut down the backend and exit the process. */
	[[noreturn]] virtual void quit() = 0;

	/** @} */

protected:
	std::unique_ptr<Common::EventManager> _eventManager;
	std::unique_ptr<Common::SaveFileManager> _savefileManager;
};

/** The single backend instance, set by the platform's main(). */
extern OSystem *g_system;

#endif
