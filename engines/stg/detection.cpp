/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/metaengine.h"

#include <algorithm>
#include <cctype>
#include <cstring>

#include "engines/detection.h"
#include "engines/stg/detection_tables.h"
#include "engines/stg/disasm.h"
#include "engines/stg/script.h"
#include "engines/stg/stg.h"

namespace STG {

namespace {

/** Search a buffer for an ASCII string or its UTF-16LE form, ignoring case. */
bool containsString(const std::vector<byte> &data, const char *needle) {
	size_t len = strlen(needle);
	auto matchAt = [&](size_t pos, size_t stride) {
		for (size_t i = 0; i < len; i++) {
			size_t p = pos + i * stride;
			if (tolower(data[p]) != tolower((unsigned char)needle[i]))
				return false;
			if (stride == 2 && data[p + 1] != 0)
				return false;
		}
		return true;
	};

	for (size_t pos = 0; pos + len <= data.size(); pos++) {
		if (matchAt(pos, 1))
			return true;
		if (pos + len * 2 <= data.size() && matchAt(pos, 2))
			return true;
	}
	return false;
}

/**
 * Used when no table entry matches: look for a DOS/Windows executable that
 * mentions the game's title. This finds copies we have no checksums for,
 * which are then reported as unknown variants.
 */
DetectedGames fallbackDetect(const Common::FSNode &dir) {
	DetectedGames results;

	Common::FSList files;
	dir.getChildren(files, Common::FSNode::kListFilesOnly);
	for (const Common::FSNode &file : files) {
		if (!Common::hasSuffixIgnoreCase(file.getName(), ".exe"))
			continue;
		int64 size = file.getFileSize();
		if (size < 2 || size > 16 * 1024 * 1024)
			continue;

		std::unique_ptr<Common::SeekableReadStream> stream = file.createReadStream();
		if (!stream)
			continue;
		std::vector<byte> data = stream->readAll();
		if (data.size() < 2 || data[0] != 'M' || data[1] != 'Z')
			continue;

		if (!containsString(data, "Generations"))
			continue;

		DetectedGame game;
		game.engineId = "stg";
		game.gameId = "stg";
		game.description = stgGames[0].description;
		// A PE header means the Windows build; otherwise assume DOS.
		bool isPE = false;
		if (data.size() >= 0x40) {
			uint32 peOffset = READ_LE_UINT32(&data[0x3C]);
			isPE = peOffset + 4 <= data.size() && READ_BE_UINT32(&data[peOffset]) == MKTAG('P', 'E', 0, 0);
		}
		game.platform = isPE ? kPlatformWindows : kPlatformDOS;
		game.path = dir;
		game.unknownVariant = true;
		game.matchedFiles.push_back(describeFile(file, file.getName()));
		results.push_back(game);
		break;
	}
	return results;
}

} // End of anonymous namespace

class STGMetaEngine : public MetaEngine {
public:
	const char *getId() const override { return "stg"; }
	const char *getName() const override { return "Star Trek: Generations"; }
	const char *getOriginalCopyright() const override {
		return "Star Trek: Generations (C) 1997 MicroProse / Paramount Pictures";
	}

	std::vector<PlainGameDescriptor> getSupportedGames() const override {
		std::vector<PlainGameDescriptor> games;
		for (const PlainGameDescriptor *g = stgGames; g->gameId; g++)
			games.push_back(*g);
		return games;
	}

	DetectedGames detectGames(const Common::FSNode &dir) const override {
		DetectedGames games = detectGamesFromTable(dir, gameDescriptions, getId(), stgGames);
		bool haveExact = std::any_of(games.begin(), games.end(), [](const DetectedGame &g) { return !g.unknownVariant; });
		if (!haveExact) {
			DetectedGames fallback = fallbackDetect(dir);
			if (!fallback.empty())
				return fallback;
		}
		return games;
	}

	bool describeFile(Common::SeekableReadStream &stream, bool verbose, Common::String &out) const override {
		if (stream.size() < 4 || stream.readUint32BE() != MKTAG('S', 'G', 'E', 'N'))
			return false;
		stream.seek(0);
		ScriptFile script;
		if (!script.load(stream)) {
			out = "SGEN script (could not be parsed)";
			return true;
		}
		size_t states = 0, blocks = 0;
		for (const ScriptObject &object : script.getObjects()) {
			states += object.states.size();
			for (const ScriptState &state : object.states)
				blocks += state.blocks.size();
		}
		out = Common::format("SGEN script v%u: %d objects, %d states, %d event blocks, %d words of code",
		                     script.getVersion(), (int)script.getObjects().size(), (int)states, (int)blocks,
		                     (int)script.getCode().size());
		out += "\n  resources:";
		for (const Common::String &name : script.getResourceNames())
			out += " " + name;
		if (verbose)
			out += "\n" + disassembleScript(script);
		return true;
	}

	Common::Error createInstance(OSystem *syst, const DetectedGame &game,
	                             std::unique_ptr<Engine> &engine) const override {
		engine.reset(new STGEngine(syst, game));
		return Common::kNoError;
	}
};

} // End of namespace STG

const MetaEngine *getSTGMetaEngine() {
	static STG::STGMetaEngine metaEngine;
	return &metaEngine;
}
