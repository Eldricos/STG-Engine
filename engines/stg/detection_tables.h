/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_DETECTION_TABLES_H
#define STG_DETECTION_TABLES_H

#include "engines/detection.h"

namespace STG {

static const PlainGameDescriptor stgGames[] = {
	{ "stg", "Star Trek: Generations" },
	{ nullptr, nullptr }
};

/**
 * Known releases.
 *
 * No release has been verified yet, so the table is empty and detection
 * relies on the fallback in detection.cpp. To add a release, run
 *
 *     stg --dump-files --path=/path/to/game
 *
 * and add an entry naming two or three files unique to it, e.g.
 *
 *     {
 *         "stg", "CD",
 *         {
 *             { "SOMEFILE.EXE", "<md5 of first 5000 bytes>", <size> },
 *             { "SOMEDATA.DAT", "<md5 of first 5000 bytes>", <size> },
 *             { nullptr, nullptr, 0 }
 *         },
 *         kLangEnglish, kPlatformWindows, GF_CD | GF_UNSTABLE
 *     },
 */
static const GameDescription gameDescriptions[] = {
	{ nullptr, nullptr, { { nullptr, nullptr, 0 } }, kLangUnknown, kPlatformUnknown, GF_NONE }
};

} // End of namespace STG

#endif
