/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_ERROR_H
#define COMMON_ERROR_H

#include <string>

namespace Common {

enum ErrorCode {
	kNoError = 0,
	kNoGameDataFoundError,
	kUnsupportedGameidError,
	kUnsupportedColorMode,
	kAudioDeviceInitFailed,
	kReadingFailed,
	kWritingFailed,
	kPathDoesNotExist,
	kPathNotDirectory,
	kUserCanceled,
	kUnknownError
};

class Error {
public:
	Error(ErrorCode code = kNoError, const std::string &extra = std::string()) : _code(code), _extra(extra) {}

	ErrorCode getCode() const { return _code; }
	bool failed() const { return _code != kNoError; }
	std::string getDesc() const;

private:
	ErrorCode _code;
	std::string _extra;
};

} // End of namespace Common

#endif
