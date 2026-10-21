/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef ENGINES_DETECTION_H
#define ENGINES_DETECTION_H

#include "engines/game.h"

/**
 * Table driven game detection, modelled on ScummVM's AdvancedDetector.
 * An engine lists every known release as a GameDescription; each one names
 * a few files with their sizes and the MD5 of their first kMD5Bytes bytes.
 */

/** How many leading bytes of each file are hashed. */
static const uint32 kDetectionMD5Bytes = 5000;

struct GameFileDescription {
	/** File name, case-insensitive; may include a subdirectory, e.g. "DATA/FOO.DAT". */
	const char *fileName;
	/** MD5 of the first kDetectionMD5Bytes bytes, or nullptr to accept any. */
	const char *md5;
	/** Exact file size, or -1 to accept any. */
	int64 fileSize;
};

enum GameFlags {
	GF_NONE = 0,
	GF_DEMO = 1 << 0,
	GF_CD = 1 << 1,
	/** Detected but not yet playable; the launcher warns before starting. */
	GF_UNSTABLE = 1 << 2
};

static const int kMaxDetectionFiles = 8;

struct GameDescription {
	const char *gameId;
	/** Distinguishes releases, e.g. "CD" or "Demo". */
	const char *extra;
	/** Terminated by an entry with a null fileName. */
	GameFileDescription files[kMaxDetectionFiles];
	GameLanguage language;
	GamePlatform platform;
	uint32 flags;
};

/**
 * Match a directory against a detection table. Returns one DetectedGame for
 * every entry whose files all match; DetectedGame::engineData points at
 * the matching GameDescription.
 *
 * If no entry matches exactly but every file of an entry is present (with a
 * different size or checksum), an unknown variant is returned for the
 * closest entry so the user can report it.
 */
DetectedGames detectGamesFromTable(const Common::FSNode &dir, const GameDescription *table,
                                   const char *engineId, const PlainGameDescriptor *gameNames);

/** Size and partial MD5 of a file, as used by the detection tables. */
DetectedFile describeFile(const Common::FSNode &file, const Common::String &name);

#endif
