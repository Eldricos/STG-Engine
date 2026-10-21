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
 * Known releases. Each entry matches files in the *installed* game
 * directory, since the executable is only on the CDs inside the installer.
 *
 * To add a release, run
 *
 *     stg --dump-files --path=/path/to/game
 *
 * and add an entry naming two or three files unique to it, with their
 * sizes and the MD5 of their first 5000 bytes.
 */
static const GameDescription gameDescriptions[] = {
	// 2 CD Windows release, installed. Language from the README; not yet
	// confirmed against a localised copy.
	{
		"stg", "CD",
		{
			{ "sgens.exe", "5044cd4e8cd2ef067dc79d86b38cc44d", 1571328 },
			{ "DATA/COMMON.DAT", "fc40c9b9bf0dad97830037a4d7a7483f", 11410717 },
			{ nullptr, nullptr, 0 }
		},
		kLangEnglish, kPlatformWindows, GF_CD | GF_UNSTABLE
	},

	{ nullptr, nullptr, { { nullptr, nullptr, 0 } }, kLangUnknown, kPlatformUnknown, GF_NONE }
};

} // End of namespace STG

#endif
