/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "common/system.h"
#include "common/config-manager.h"
#include "common/graphics/surface.h"

#include <cstdio>

OSystem *g_system = nullptr;

OSystem::OSystem() {
}

OSystem::~OSystem() {
	Common::setLogHandler(nullptr);
	if (g_system == this)
		g_system = nullptr;
}

void OSystem::initBackend() {
	_eventManager.reset(new Common::EventManager(this));

	Common::String savePath = ConfMan.hasKey("savepath") ? ConfMan.get("savepath") : getDefaultSavePath();
	_savefileManager.reset(new Common::SaveFileManager(Common::FSNode(savePath)));

	Common::setLogHandler([this](Common::LogType type, const char *message) {
		logMessage(type, message);
	});
}

std::vector<Graphics::PixelFormat> OSystem::getSupportedFormats() const {
	return { Graphics::PixelFormat::createFormatCLUT8() };
}

void OSystem::fillScreen(uint32 color) {
	Graphics::Surface *screen = lockScreen();
	if (screen)
		screen->fillRect(Common::Rect(screen->w, screen->h), color);
	unlockScreen();
}

void OSystem::logMessage(Common::LogType type, const char *message) {
	static const char *const prefixes[] = { "", "", "WARNING: ", "ERROR: " };
	FILE *out = type == Common::kLogDebug || type == Common::kLogInfo ? stdout : stderr;
	fprintf(out, "%s%s\n", prefixes[type], message);
	fflush(out);
}
