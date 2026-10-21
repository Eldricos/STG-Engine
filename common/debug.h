/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_DEBUG_H
#define COMMON_DEBUG_H

#include <functional>

namespace Common {

enum LogType {
	kLogDebug,
	kLogInfo,
	kLogWarning,
	kLogError
};

typedef std::function<void(LogType type, const char *message)> LogHandler;

/**
 * Route all log output through the given handler. The backend installs one
 * at startup so that output ends up wherever is appropriate for the platform.
 * Until then messages go to stderr.
 */
void setLogHandler(LogHandler handler);

void setDebugLevel(int level);
int getDebugLevel();

/**
 * Debug channels let engines enable verbose output per subsystem,
 * e.g. --debugflags=resources,sound
 */
void enableDebugChannel(const char *name);
bool isDebugChannelEnabled(const char *name);

} // End of namespace Common

void debug(const char *fmt, ...);
void debug(int level, const char *fmt, ...);
void debugC(int level, const char *channel, const char *fmt, ...);
void info(const char *fmt, ...);
void warning(const char *fmt, ...);

/** Print an error message and terminate. */
[[noreturn]] void error(const char *fmt, ...);

#endif
