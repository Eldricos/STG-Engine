/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "sdl/sdl-system.h"

#include <SDL3/SDL.h>

#include <cstdlib>

#include "common/config-manager.h"
#include "sdl/sdl-events.h"
#include "sdl/sdl-graphics.h"
#include "sdl/sdl-mixer.h"

#ifndef STG_VERSION
#define STG_VERSION "0.1.0"
#endif

OSystem_SDL::OSystem_SDL() {
}

OSystem_SDL::~OSystem_SDL() {
	shutdown();
}

void OSystem_SDL::shutdown() {
	// Audio first: its callback thread uses the mixer.
	_mixer.reset();
	_events.reset();
	_graphics.reset();
	if (_initialized) {
		SDL_Quit();
		_initialized = false;
	}
}

void OSystem_SDL::initBackend() {
	SDL_SetAppMetadata("STG-Engine", STG_VERSION, "org.stg-engine.stg");

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
		error("Could not initialise SDL: %s", SDL_GetError());
	_initialized = true;

	_graphics.reset(new SdlGraphicsManager());
	_events.reset(new SdlEventSource(_graphics.get()));
	_mixer.reset(new SdlMixerManager());
	_mixer->init();

	OSystem::initBackend();

	_graphics->setFullscreen(ConfMan.getBool("fullscreen"));
	_graphics->setFiltering(ConfMan.getBool("filtering"));
	_graphics->setIntegerScaling(ConfMan.getBool("integer_scaling"));

	int version = SDL_GetVersion();
	debug(1, "SDL %d.%d.%d, video driver '%s'", SDL_VERSIONNUM_MAJOR(version), SDL_VERSIONNUM_MINOR(version),
	      SDL_VERSIONNUM_MICRO(version), SDL_GetCurrentVideoDriver());
}

bool OSystem_SDL::hasFeature(Feature f) const {
	switch (f) {
	case kFeatureFullscreenMode:
	case kFeatureFilteringMode:
	case kFeatureIntegerScaling:
	case kFeatureMessageBox:
		return true;
	default:
		return false;
	}
}

void OSystem_SDL::setFeatureState(Feature f, bool enable) {
	switch (f) {
	case kFeatureFullscreenMode:
		_graphics->setFullscreen(enable);
		break;
	case kFeatureFilteringMode:
		_graphics->setFiltering(enable);
		break;
	case kFeatureIntegerScaling:
		_graphics->setIntegerScaling(enable);
		break;
	default:
		break;
	}
}

bool OSystem_SDL::getFeatureState(Feature f) const {
	switch (f) {
	case kFeatureFullscreenMode:
		return _graphics->isFullscreen();
	case kFeatureFilteringMode:
		return _graphics->isFiltering();
	case kFeatureIntegerScaling:
		return _graphics->isIntegerScaling();
	default:
		return false;
	}
}

std::vector<Graphics::PixelFormat> OSystem_SDL::getSupportedFormats() const {
	return {
		Graphics::PixelFormat::createFormatARGB8888(),
		Graphics::PixelFormat::createFormatRGB565(),
		Graphics::PixelFormat::createFormatRGB555(),
		Graphics::PixelFormat::createFormatCLUT8()
	};
}

void OSystem_SDL::initSize(uint width, uint height, const Graphics::PixelFormat *format) {
	_graphics->initSize(width, height, format ? *format : Graphics::PixelFormat::createFormatCLUT8());
}

int16 OSystem_SDL::getWidth() const {
	return _graphics->getWidth();
}

int16 OSystem_SDL::getHeight() const {
	return _graphics->getHeight();
}

Graphics::PixelFormat OSystem_SDL::getScreenFormat() const {
	return _graphics->getScreenFormat();
}

void OSystem_SDL::setPalette(const byte *colors, uint start, uint num) {
	_graphics->setPalette(colors, start, num);
}

void OSystem_SDL::grabPalette(byte *colors, uint start, uint num) const {
	_graphics->grabPalette(colors, start, num);
}

void OSystem_SDL::copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h) {
	_graphics->copyRectToScreen(buf, pitch, x, y, w, h);
}

Graphics::Surface *OSystem_SDL::lockScreen() {
	return _graphics->lockScreen();
}

void OSystem_SDL::unlockScreen() {
	_graphics->unlockScreen();
}

void OSystem_SDL::updateScreen() {
	_graphics->updateScreen();
}

bool OSystem_SDL::showMouse(bool visible) {
	return _graphics->showMouse(visible);
}

void OSystem_SDL::warpMouse(int x, int y) {
	_graphics->warpMouse(x, y);
}

void OSystem_SDL::setMouseCursor(const void *buf, uint w, uint h, int hotspotX, int hotspotY, uint32 keycolor,
                                 const Graphics::PixelFormat *format) {
	_graphics->setMouseCursor(buf, w, h, hotspotX, hotspotY, keycolor,
	                          format ? *format : Graphics::PixelFormat::createFormatCLUT8());
}

bool OSystem_SDL::pollEvent(Common::Event &event) {
	return _events->pollEvent(event);
}

uint32 OSystem_SDL::getMillis() const {
	return (uint32)SDL_GetTicks();
}

void OSystem_SDL::delayMillis(uint msecs) {
	SDL_Delay(msecs);
}

Audio::Mixer *OSystem_SDL::getMixer() {
	return _mixer ? _mixer->getMixer() : nullptr;
}

Common::String OSystem_SDL::getUserDataPath() const {
	if (_userDataPath.empty()) {
		// SDL picks the platform's convention, e.g. ~/.local/share/STG-Engine/stg/
		// on Linux or %APPDATA%\STG-Engine\stg\ on Windows, and creates it.
		char *path = SDL_GetPrefPath("STG-Engine", "stg");
		if (path) {
			_userDataPath = path;
			SDL_free(path);
		} else {
			_userDataPath = "./";
		}
	}
	return _userDataPath;
}

Common::String OSystem_SDL::getDefaultSavePath() const {
	return getUserDataPath() + "saves";
}

Common::String OSystem_SDL::getDefaultConfigFileName() const {
	return getUserDataPath() + "stg.ini";
}

void OSystem_SDL::setWindowCaption(const Common::String &caption) {
	if (_graphics)
		_graphics->setWindowCaption(caption);
}

void OSystem_SDL::logMessage(Common::LogType type, const char *message) {
	OSystem::logMessage(type, message);
	if (type == Common::kLogError)
		messageBox(type, message);
}

void OSystem_SDL::messageBox(Common::LogType type, const char *message) {
	SDL_MessageBoxFlags flags = type == Common::kLogError     ? SDL_MESSAGEBOX_ERROR
	                            : type == Common::kLogWarning ? SDL_MESSAGEBOX_WARNING
	                                                          : SDL_MESSAGEBOX_INFORMATION;
	SDL_ShowSimpleMessageBox(flags, "STG-Engine", message, _graphics ? _graphics->getWindow() : nullptr);
}

void OSystem_SDL::quit() {
	shutdown();
	exit(0);
}
