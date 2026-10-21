/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "common/error.h"

namespace Common {

static const char *errorToString(ErrorCode code) {
	switch (code) {
	case kNoError:
		return "No error";
	case kNoGameDataFoundError:
		return "Could not find any game data in the specified directory";
	case kUnsupportedGameidError:
		return "Game id not supported";
	case kUnsupportedColorMode:
		return "Unsupported color mode";
	case kAudioDeviceInitFailed:
		return "Audio device initialization failed";
	case kReadingFailed:
		return "Game data read failed";
	case kWritingFailed:
		return "Data write failed";
	case kPathDoesNotExist:
		return "Path does not exist";
	case kPathNotDirectory:
		return "Path is not a directory";
	case kUserCanceled:
		return "Operation canceled by user";
	case kUnknownError:
	default:
		return "Unknown error";
	}
}

std::string Error::getDesc() const {
	std::string desc = errorToString(_code);
	if (!_extra.empty())
		desc += " (" + _extra + ")";
	return desc;
}

} // End of namespace Common
