/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "common/debug.h"
#include "common/str.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>

namespace Common {

static LogHandler g_logHandler;
static int g_debugLevel = 0;
static std::set<std::string> g_debugChannels;

void setLogHandler(LogHandler handler) {
	g_logHandler = handler;
}

void setDebugLevel(int level) {
	g_debugLevel = level;
}

int getDebugLevel() {
	return g_debugLevel;
}

void enableDebugChannel(const char *name) {
	g_debugChannels.insert(toLower(name));
}

bool isDebugChannelEnabled(const char *name) {
	return g_debugChannels.count("all") || g_debugChannels.count(toLower(name));
}

static void logMessage(LogType type, const char *fmt, va_list args) {
	char buf[4096];
	vsnprintf(buf, sizeof(buf), fmt, args);

	if (g_logHandler) {
		g_logHandler(type, buf);
		return;
	}

	static const char *const prefixes[] = { "", "", "WARNING: ", "ERROR: " };
	fprintf(stderr, "%s%s\n", prefixes[type], buf);
}

} // End of namespace Common

void debug(const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	Common::logMessage(Common::kLogDebug, fmt, va);
	va_end(va);
}

void debug(int level, const char *fmt, ...) {
	if (level > Common::g_debugLevel)
		return;
	va_list va;
	va_start(va, fmt);
	Common::logMessage(Common::kLogDebug, fmt, va);
	va_end(va);
}

void debugC(int level, const char *channel, const char *fmt, ...) {
	if (level > Common::g_debugLevel || !Common::isDebugChannelEnabled(channel))
		return;
	va_list va;
	va_start(va, fmt);
	Common::logMessage(Common::kLogDebug, fmt, va);
	va_end(va);
}

void info(const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	Common::logMessage(Common::kLogInfo, fmt, va);
	va_end(va);
}

void warning(const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	Common::logMessage(Common::kLogWarning, fmt, va);
	va_end(va);
}

void error(const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	Common::logMessage(Common::kLogError, fmt, va);
	va_end(va);
	exit(1);
}
