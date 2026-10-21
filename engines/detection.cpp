/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/detection.h"

#include <map>

#include "common/archive.h"
#include "common/md5.h"

DetectedFile describeFile(const Common::FSNode &file, const Common::String &name) {
	DetectedFile result;
	result.name = name;
	result.size = file.getFileSize();
	std::unique_ptr<Common::SeekableReadStream> stream = file.createReadStream();
	if (stream)
		result.md5 = Common::computeStreamMD5(*stream, kDetectionMD5Bytes);
	return result;
}

static Common::String lookupGameName(const PlainGameDescriptor *gameNames, const char *gameId) {
	for (const PlainGameDescriptor *g = gameNames; g && g->gameId; g++) {
		if (Common::equalsIgnoreCase(g->gameId, gameId))
			return g->description;
	}
	return gameId;
}

DetectedGames detectGamesFromTable(const Common::FSNode &dir, const GameDescription *table,
                                   const char *engineId, const PlainGameDescriptor *gameNames) {
	DetectedGames results;
	if (!dir.isDirectory())
		return results;

	// Two levels deep is enough for CD layouts with a data subdirectory.
	Common::FSDirectory files(dir, 2);
	std::map<Common::String, DetectedFile> cache;

	auto getFile = [&](const char *name, DetectedFile &out) {
		Common::String key = Common::toLower(name);
		auto it = cache.find(key);
		if (it == cache.end()) {
			Common::ArchiveMemberPtr member = files.getMember(name);
			if (!member)
				return false;
			// Resolve the member back to a host node so we can size and hash it.
			Common::FSNode node = dir;
			for (const Common::String &part : Common::split(member->getName(), '/'))
				node = node.getChild(part);
			it = cache.emplace(key, describeFile(node, member->getName())).first;
		}
		out = it->second;
		return true;
	};

	const GameDescription *closest = nullptr;
	std::vector<DetectedFile> closestFiles;

	for (const GameDescription *desc = table; desc->gameId; desc++) {
		bool allPresent = true;
		bool allMatch = true;
		std::vector<DetectedFile> matched;

		for (const GameFileDescription *f = desc->files; f->fileName; f++) {
			DetectedFile file;
			if (!getFile(f->fileName, file)) {
				allPresent = allMatch = false;
				break;
			}
			if ((f->fileSize != -1 && f->fileSize != file.size) || (f->md5 && file.md5 != f->md5))
				allMatch = false;
			matched.push_back(file);
		}

		if (allMatch) {
			DetectedGame game;
			game.engineId = engineId;
			game.gameId = desc->gameId;
			game.description = lookupGameName(gameNames, desc->gameId);
			game.extra = desc->extra ? desc->extra : "";
			game.platform = desc->platform;
			game.language = desc->language;
			game.path = dir;
			game.matchedFiles = matched;
			game.engineData = desc;
			results.push_back(game);
		} else if (allPresent && !closest) {
			closest = desc;
			closestFiles = matched;
		}
	}

	if (results.empty() && closest) {
		DetectedGame game;
		game.engineId = engineId;
		game.gameId = closest->gameId;
		game.description = lookupGameName(gameNames, closest->gameId);
		game.platform = closest->platform;
		game.language = closest->language;
		game.path = dir;
		game.unknownVariant = true;
		game.matchedFiles = closestFiles;
		results.push_back(game);
	}

	return results;
}
