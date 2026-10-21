/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef ENGINES_GAME_H
#define ENGINES_GAME_H

#include <vector>

#include "common/fs.h"
#include "common/str.h"
#include "common/types.h"

enum GamePlatform {
	kPlatformUnknown = 0,
	kPlatformDOS,
	kPlatformWindows
};

enum GameLanguage {
	kLangUnknown = 0,
	kLangEnglish,
	kLangGerman,
	kLangFrench,
	kLangItalian,
	kLangSpanish
};

const char *getPlatformDescription(GamePlatform platform);
const char *getLanguageDescription(GameLanguage language);

/** A game an engine knows how to run, independent of any particular copy. */
struct PlainGameDescriptor {
	const char *gameId;
	const char *description;
};

/** A data file seen during detection, for "please report this version" messages. */
struct DetectedFile {
	Common::String name;
	int64 size;
	Common::String md5;
};

/** A copy of a game found on disk. */
struct DetectedGame {
	Common::String engineId;
	Common::String gameId;
	Common::String description;
	Common::String extra;
	GamePlatform platform = kPlatformUnknown;
	GameLanguage language = kLangUnknown;
	Common::FSNode path;

	/**
	 * True if the files look like this game but don't match any known
	 * version exactly. The engine may still try to run it.
	 */
	bool unknownVariant = false;
	std::vector<DetectedFile> matchedFiles;

	/** Engine-private pointer to the matching detection table entry, if any. */
	const void *engineData = nullptr;

	Common::String getFullDescription() const;
	/** A default config target name, e.g. "stg-win". */
	Common::String getPreferredTarget() const;
};

typedef std::vector<DetectedGame> DetectedGames;

#endif
