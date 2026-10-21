/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/launcher.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "common/archive.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/system.h"
#include "engines/detection.h"
#include "engines/engine.h"
#include "engines/metaengine.h"
#include "engines/probe.h"

#ifndef STG_VERSION
#define STG_VERSION "0.1.0"
#endif

namespace {

const char *const kHelpString =
	"Usage: stg [OPTIONS] [TARGET | GAME_PATH]\n"
	"\n"
	"Commands:\n"
	"  -h, --help               Show this help and exit\n"
	"  -v, --version            Show version information and exit\n"
	"  --list-engines           List the engines built into this binary\n"
	"  --list-games             List the games the engines support\n"
	"  --list-targets           List the game targets in the config file\n"
	"  --detect                 Detect games in --path (or the current directory)\n"
	"  --add                    Detect a game in --path and add it to the config file\n"
	"  --dump-files             Print size and detection MD5 of every file in --path\n"
	"  --probe                  Identify the format of every file in --path (or one file)\n"
	"\n"
	"Options:\n"
	"  -p, --path=PATH          Directory containing the game data\n"
	"  -e, --engine=ID          Only consider this engine when detecting\n"
	"      --game=ID            Force a game id when detection does not recognise the files\n"
	"  -f, --fullscreen         Start in fullscreen mode\n"
	"  -F, --no-fullscreen      Start in windowed mode\n"
	"      --filtering          Use bilinear filtering when scaling\n"
	"      --integer-scaling    Only scale by whole multiples\n"
	"      --savepath=PATH      Directory for saved games\n"
	"      --probe-bytes=NUM    Bytes to hex dump per unknown file with --probe\n"
	"      --cd1=PATH           Disc 1 (or a copy of it), for files not installed to disk\n"
	"      --cd2=PATH           Disc 2 (or a copy of it)\n"
	"  -c, --config=FILE        Use an alternate configuration file\n"
	"  -d, --debuglevel=NUM     Set debug verbosity\n"
	"      --debugflags=A,B     Enable engine debug channels (or 'all')\n"
	"      --music-volume=NUM   Music volume, 0-256\n"
	"      --sfx-volume=NUM     Sound effects volume, 0-256\n"
	"      --speech-volume=NUM  Speech volume, 0-256\n"
	"\n"
	"With no argument the game in the current directory is started.\n";

struct Command {
	Common::String name;
	Common::String target;
};

/** Split "--key=value" or "-k value" style options. Returns false on error. */
bool parseCommandLine(int argc, char *argv[], Command &command) {
	struct Option {
		const char *shortName;
		const char *longName;
		const char *configKey;
		bool takesValue;
		const char *flagValue;
	};
	static const Option options[] = {
		{ "p", "path", "path", true, nullptr },
		{ "e", "engine", "engineid", true, nullptr },
		{ nullptr, "game", "gameid", true, nullptr },
		{ "f", "fullscreen", "fullscreen", false, "true" },
		{ "F", "no-fullscreen", "fullscreen", false, "false" },
		{ nullptr, "filtering", "filtering", false, "true" },
		{ nullptr, "integer-scaling", "integer_scaling", false, "true" },
		{ nullptr, "savepath", "savepath", true, nullptr },
		{ nullptr, "probe-bytes", "probe_bytes", true, nullptr },
		{ nullptr, "cd1", "cd1path", true, nullptr },
		{ nullptr, "cd2", "cd2path", true, nullptr },
		{ "c", "config", "config", true, nullptr },
		{ "d", "debuglevel", "debuglevel", true, nullptr },
		{ nullptr, "debugflags", "debugflags", true, nullptr },
		{ nullptr, "music-volume", "music_volume", true, nullptr },
		{ nullptr, "sfx-volume", "sfx_volume", true, nullptr },
		{ nullptr, "speech-volume", "speech_volume", true, nullptr },
	};
	static const char *const commands[] = {
		"help", "version", "list-engines", "list-games", "list-targets", "detect", "add", "dump-files", "probe"
	};

	for (int i = 1; i < argc; i++) {
		Common::String arg = argv[i];

		if (arg == "-h")
			arg = "--help";
		else if (arg == "-v")
			arg = "--version";

		if (arg.size() < 2 || arg[0] != '-') {
			if (!command.target.empty()) {
				fprintf(stderr, "Only one target or game path may be given\n");
				return false;
			}
			command.target = arg;
			continue;
		}

		bool isLong = arg[1] == '-';
		Common::String name = arg.substr(isLong ? 2 : 1);
		Common::String value;
		bool hasValue = false;
		size_t eq = name.find('=');
		if (isLong && eq != Common::String::npos) {
			value = name.substr(eq + 1);
			name = name.substr(0, eq);
			hasValue = true;
		} else if (!isLong && name.size() > 1) {
			// Attached short option value, e.g. -d5
			value = name.substr(1);
			name = name.substr(0, 1);
			hasValue = true;
		}

		bool handled = false;
		if (isLong) {
			for (const char *cmd : commands) {
				if (name == cmd) {
					command.name = name;
					handled = true;
				}
			}
		}

		for (const Option &opt : options) {
			if (handled)
				break;
			if (isLong ? name != opt.longName : (!opt.shortName || name != opt.shortName))
				continue;
			handled = true;
			if (opt.takesValue) {
				if (!hasValue) {
					if (i + 1 >= argc) {
						fprintf(stderr, "Option '%s' requires a value\n", arg.c_str());
						return false;
					}
					value = argv[++i];
				}
				ConfMan.setTransient(opt.configKey, value);
			} else {
				ConfMan.setTransient(opt.configKey, opt.flagValue);
			}
		}

		if (!handled) {
			fprintf(stderr, "Unrecognised option '%s'\n", arg.c_str());
			return false;
		}
	}
	return true;
}

void registerDefaults() {
	ConfMan.registerDefault("fullscreen", false);
	ConfMan.registerDefault("filtering", false);
	ConfMan.registerDefault("integer_scaling", false);
	ConfMan.registerDefault("music_volume", 192);
	ConfMan.registerDefault("sfx_volume", 192);
	ConfMan.registerDefault("speech_volume", 192);
	ConfMan.registerDefault("mute", false);
	ConfMan.registerDefault("debuglevel", 0);

	for (const MetaEngine *engine : getMetaEngines())
		engine->registerDefaultSettings();
}

void applyDebugSettings() {
	Common::setDebugLevel(ConfMan.getInt("debuglevel"));
	for (const Common::String &channel : Common::split(ConfMan.get("debugflags"), ',')) {
		if (!Common::trim(channel).empty())
			Common::enableDebugChannel(Common::trim(channel).c_str());
	}
}

DetectedGames detectAll(const Common::FSNode &dir) {
	DetectedGames results;
	Common::String engineId = ConfMan.get("engineid");
	for (const MetaEngine *engine : getMetaEngines()) {
		if (!engineId.empty() && !Common::equalsIgnoreCase(engineId, engine->getId()))
			continue;
		DetectedGames games = engine->detectGames(dir);
		results.insert(results.end(), games.begin(), games.end());
	}

	// Prefer exact matches to unknown variants.
	bool haveExact = false;
	for (const DetectedGame &game : results)
		haveExact |= !game.unknownVariant;
	if (haveExact) {
		DetectedGames exact;
		for (const DetectedGame &game : results) {
			if (!game.unknownVariant)
				exact.push_back(game);
		}
		results = exact;
	}
	return results;
}

void printUnknownVariant(const DetectedGame &game) {
	printf("The game in '%s' looks like %s, but this version is not in the detection table.\n",
	       game.path.getPath().c_str(), game.description.c_str());
	printf("Please report the following details so it can be added:\n");
	for (const DetectedFile &file : game.matchedFiles)
		printf("  {\"%s\", \"%s\", %lld},\n", file.name.c_str(), file.md5.c_str(), (long long)file.size);
}

int listEngines() {
	printf("Engine ID   Engine name\n");
	printf("----------- ------------------------------------------\n");
	for (const MetaEngine *engine : getMetaEngines())
		printf("%-11s %s\n", engine->getId(), engine->getName());
	return 0;
}

int listGames() {
	printf("Game ID              Full title\n");
	printf("-------------------- ---------------------------------------\n");
	for (const MetaEngine *engine : getMetaEngines()) {
		for (const PlainGameDescriptor &game : engine->getSupportedGames())
			printf("%-20s %s\n", (Common::String(engine->getId()) + ":" + game.gameId).c_str(), game.description);
	}
	return 0;
}

int listTargets() {
	printf("Target               Description\n");
	printf("-------------------- ---------------------------------------\n");
	for (const Common::String &target : ConfMan.getGameDomains())
		printf("%-20s %s\n", target.c_str(), ConfMan.get("description", target).c_str());
	return 0;
}

Common::FSNode getPathArgument(const Command &command) {
	Common::String path = ConfMan.get("path");
	if (path.empty())
		path = command.target;
	if (path.empty())
		path = ".";
	return Common::FSNode(path);
}

int detectCommand(const Command &command) {
	Common::FSNode dir = getPathArgument(command);
	if (!dir.isDirectory()) {
		fprintf(stderr, "'%s' is not a directory\n", dir.getPath().c_str());
		return 1;
	}
	DetectedGames games = detectAll(dir);
	if (games.empty()) {
		printf("No supported game found in '%s'\n", dir.getPath().c_str());
		return 1;
	}
	printf("GameID               Description\n");
	printf("-------------------- ---------------------------------------\n");
	for (const DetectedGame &game : games)
		printf("%-20s %s\n", (game.engineId + ":" + game.gameId).c_str(), game.getFullDescription().c_str());
	for (const DetectedGame &game : games) {
		if (game.unknownVariant)
			printUnknownVariant(game);
	}
	return 0;
}

int dumpFilesCommand(const Command &command) {
	Common::FSNode dir = getPathArgument(command);
	if (!dir.isDirectory()) {
		fprintf(stderr, "'%s' is not a directory\n", dir.getPath().c_str());
		return 1;
	}

	Common::FSDirectory archive(dir, 4);
	Common::ArchiveMemberList members;
	archive.listMembers(members);
	printf("%-40s %12s  %s\n", "File", "Size", "MD5 (first 5000 bytes)");
	for (const Common::ArchiveMemberPtr &member : members) {
		Common::FSNode node = dir;
		for (const Common::String &part : Common::split(member->getName(), '/'))
			node = node.getChild(part);
		DetectedFile file = describeFile(node, member->getName());
		printf("%-40s %12lld  %s\n", file.name.c_str(), (long long)file.size, file.md5.c_str());
	}
	return 0;
}

int probeCommand(const Command &command) {
	Common::FSNode path = getPathArgument(command);
	if (!path.exists()) {
		fprintf(stderr, "'%s' does not exist\n", path.getPath().c_str());
		return 1;
	}

	int bytes = ConfMan.getInt("probe_bytes");
	if (!path.isDirectory()) {
		printf("%s\n", probeFile(path, path.getName(), bytes > 0 ? bytes : 512).c_str());
		return 0;
	}

	Common::FSDirectory archive(path, 4);
	Common::ArchiveMemberList members;
	archive.listMembers(members);
	for (const Common::ArchiveMemberPtr &member : members) {
		Common::FSNode node = path;
		for (const Common::String &part : Common::split(member->getName(), '/'))
			node = node.getChild(part);
		printf("%s\n", probeFile(node, member->getName(), bytes > 0 ? bytes : 128).c_str());
	}
	return 0;
}

/** Pick the game to run, either from a config target or by detection. */
bool selectGame(const Command &command, DetectedGame &selected, Common::String &target) {
	if (!command.target.empty() && ConfMan.hasDomain(command.target)) {
		target = command.target;
		ConfMan.setActiveDomain(target);
	}

	Common::FSNode dir = getPathArgument(target.empty() ? command : Command());
	if (!dir.isDirectory()) {
		fprintf(stderr, "Game path '%s' is not a directory\n", dir.getPath().c_str());
		return false;
	}

	DetectedGames games = detectAll(dir);

	Common::String gameId = ConfMan.get("gameid");
	if (!gameId.empty()) {
		DetectedGames filtered;
		for (const DetectedGame &game : games) {
			if (Common::equalsIgnoreCase(game.gameId, gameId))
				filtered.push_back(game);
		}
		games = filtered;
	}

	if (games.empty()) {
		// Forcing an engine and game lets developers run data that the
		// detection table doesn't know yet.
		Common::String engineId = ConfMan.get("engineid");
		if (!engineId.empty() && !gameId.empty() && findMetaEngine(engineId)) {
			warning("No known version detected; forcing %s:%s", engineId.c_str(), gameId.c_str());
			selected.engineId = engineId;
			selected.gameId = gameId;
			selected.description = gameId;
			selected.path = dir;
			selected.unknownVariant = true;
			if (target.empty())
				target = gameId;
			return true;
		}
		fprintf(stderr, "No supported game found in '%s'.\n"
		                "Run with --dump-files to list the files there, or use --engine and --game to force one.\n",
		        dir.getPath().c_str());
		return false;
	}

	if (games.size() > 1) {
		printf("Several games were detected; starting the first. Use --game to choose:\n");
		for (const DetectedGame &game : games)
			printf("  %s:%s  %s\n", game.engineId.c_str(), game.gameId.c_str(), game.getFullDescription().c_str());
	}

	selected = games[0];
	if (selected.unknownVariant)
		printUnknownVariant(selected);
	if (target.empty())
		target = selected.getPreferredTarget();
	return true;
}

int addCommand(const Command &command) {
	Common::FSNode dir = getPathArgument(command);
	DetectedGames games = detectAll(dir);
	if (games.empty()) {
		fprintf(stderr, "No supported game found in '%s'\n", dir.getPath().c_str());
		return 1;
	}
	const DetectedGame &game = games[0];
	Common::String target = game.getPreferredTarget();
	for (int i = 2; ConfMan.hasDomain(target); i++)
		target = game.getPreferredTarget() + Common::format("-%d", i);

	ConfMan.set("engineid", game.engineId, target);
	ConfMan.set("gameid", game.gameId, target);
	ConfMan.set("description", game.getFullDescription(), target);
	ConfMan.set("path", game.path.getPath(), target);
	for (const char *key : { "cd1path", "cd2path" }) {
		if (ConfMan.hasKey(key))
			ConfMan.set(key, ConfMan.get(key), target);
	}
	if (!ConfMan.flushToDisk()) {
		fprintf(stderr, "Could not write the configuration file\n");
		return 1;
	}
	printf("Added target '%s': %s\n", target.c_str(), game.getFullDescription().c_str());
	return 0;
}

int runGame(OSystem &system, const Command &command) {
	DetectedGame game;
	Common::String target;
	if (!selectGame(command, game, target))
		return 1;

	const MetaEngine *metaEngine = findMetaEngine(game.engineId);
	if (!metaEngine) {
		fprintf(stderr, "Engine '%s' is not built into this binary\n", game.engineId.c_str());
		return 1;
	}

	if (ConfMan.getActiveDomain().empty())
		ConfMan.setActiveDomain(target);

	// Make the game directory visible to Common::File.
	SearchMan.clear();
	SearchMan.addDirectory("game", game.path, 0, 4);

	info("Running %s from '%s'", game.getFullDescription().c_str(), game.path.getPath().c_str());
	system.setWindowCaption(game.getFullDescription() + " - STG-Engine");

	std::unique_ptr<Engine> engine;
	Common::Error err = metaEngine->createInstance(&system, game, engine);
	if (!err.failed() && engine) {
		engine->syncSoundSettings();
		err = engine->run();
	}
	engine.reset();

	SearchMan.clear();
	ConfMan.flushToDisk();

	if (err.failed()) {
		Common::String message = "Error running game: " + err.getDesc();
		fprintf(stderr, "%s\n", message.c_str());
		system.messageBox(Common::kLogError, message.c_str());
		return 1;
	}
	return 0;
}

} // End of anonymous namespace

int stgMain(OSystem &system, int argc, char *argv[]) {
	g_system = &system;

	Command command;
	if (!parseCommandLine(argc, argv, command)) {
		fprintf(stderr, "Try 'stg --help' for more information.\n");
		return 1;
	}

	if (command.name == "help") {
		printf("%s", kHelpString);
		return 0;
	}
	if (command.name == "version") {
		printf("STG-Engine %s\nEngines built in:", STG_VERSION);
		for (const MetaEngine *engine : getMetaEngines())
			printf(" %s", engine->getId());
		printf("\n");
		return 0;
	}
	if (command.name == "list-engines")
		return listEngines();
	if (command.name == "list-games")
		return listGames();

	registerDefaults();

	Common::String configFile = ConfMan.get("config");
	if (configFile.empty())
		configFile = system.getDefaultConfigFileName();
	ConfMan.loadDefaultConfigFile(Common::FSNode(configFile));
	applyDebugSettings();

	if (command.name == "list-targets")
		return listTargets();
	if (command.name == "detect")
		return detectCommand(command);
	if (command.name == "dump-files")
		return dumpFilesCommand(command);
	if (command.name == "add")
		return addCommand(command);
	if (command.name == "probe")
		return probeCommand(command);

	system.initBackend();
	return runGame(system, command);
}
