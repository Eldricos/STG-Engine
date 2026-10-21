/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/resources.h"

#include <map>

#include "common/archive.h"
#include "common/audio/wave.h"
#include "common/debug.h"
#include "common/file.h"

namespace STG {

namespace {

/**
 * Which disc each data file lives on, for the 2 CD release (from the
 * disc listings). Files on both discs are identical. Files that the
 * installer copies into DATA/ are marked as installed.
 */
struct DataFileLocation {
	const char *name;
	bool installed;
	bool onDisc1;
	bool onDisc2;
};

const DataFileLocation kDataFiles[] = {
	{ "COMMON.DAT", true, true, true },
	{ "OPTIONS.DAT", true, true, true },
	{ "AMARGOS1.DAT", true, true, false },
	{ "AMARGOS2.DAT", true, false, true },
	{ "ANTILIOS.DAT", true, true, false },
	{ "ARVADA1.DAT", true, false, true },
	{ "BERSUS.DAT", true, true, false },
	{ "ENTERP.DAT", true, false, true },
	{ "EPSION.DAT", true, true, false },
	{ "GALORN1.DAT", true, false, true },
	{ "GALORN2.DAT", true, false, true },
	{ "HALEE1.DAT", true, false, true },
	{ "QURASH.DAT", true, true, false },
	{ "VERID1.DAT", true, false, true },
	{ "COMBAT.DAT", false, true, true },
	{ "STELLAR.DAT", false, true, true },
	{ "MUSIC.1", false, true, false },
	{ "MUSIC.2", false, false, true },
	{ "MOVIE/MOV_01.AVI", false, true, false },
	{ "MOVIE/MOV_02.AVI", false, true, true },
	{ "MOVIE/MOV_03.AVI", false, true, false },
	{ "MOVIE/MOV_05.AVI", false, true, false },
	{ "MOVIE/MOV_06.AVI", false, false, true },
	{ "MOVIE/MOV_07.AVI", false, true, false },
	{ "MOVIE/MOV_08.AVI", false, true, false },
	{ "MOVIE/MOV_09.AVI", false, false, true },
	{ "MOVIE/MOV_10.AVI", false, false, true },
	{ "MOVIE/MOV_13.AVI", false, false, true },
	{ "MOVIE/MOV_14.AVI", false, false, true },
	{ "MOVIE/MOV_15.AVI", false, false, true },
	{ "MOVIE/MOV_16.AVI", false, false, true },
	{ "MOVIE/MOV_18.AVI", false, true, false },
	{ nullptr, false, false, false }
};

/** The directory holding the game data inside a CD or install root, if any. */
Common::FSNode findDataDir(const Common::FSNode &root, const char *subdir) {
	Common::FSNode dir = root.getChild(subdir);
	return dir.isDirectory() ? dir : root;
}

} // End of anonymous namespace

void ResourceManager::addSearchPaths(const Common::FSNode &gameDir, const Common::String &cd1Path,
                                     const Common::String &cd2Path) {
	// Register each data directory as a root, so resources are opened by
	// their plain name ("COMMON.DAT", "MOVIE/MOV_01.AVI") wherever they are.
	// Installed files win over the CDs; both discs are searched so the game
	// never needs a disc swap.
	SearchMan.addDirectory("game", gameDir, 0, 1);
	SearchMan.addDirectory("data", findDataDir(gameDir, "DATA"), 10, 1);
	const Common::String cdPaths[2] = { cd1Path, cd2Path };
	for (int i = 0; i < 2; i++) {
		if (cdPaths[i].empty())
			continue;
		Common::FSNode cd(cdPaths[i]);
		if (!cd.isDirectory()) {
			warning("CD %d path '%s' is not a directory", i + 1, cdPaths[i].c_str());
			continue;
		}
		SearchMan.addDirectory(Common::format("cd%d", i + 1), findDataDir(cd, "GENS"), 5 - i, 2);
	}
}

bool ResourceManager::checkDataFiles() const {
	Common::StringArray missing[2];
	Common::StringArray missingInstalled;
	for (const DataFileLocation *f = kDataFiles; f->name; f++) {
		if (SearchMan.hasFile(f->name))
			continue;
		if (f->installed)
			missingInstalled.push_back(f->name);
		else if (f->onDisc1)
			missing[0].push_back(f->name);
		else
			missing[1].push_back(f->name);
	}

	if (!missingInstalled.empty())
		warning("%d installed data files not found (e.g. DATA/%s); reinstall the game or copy them from a CD's GENS folder",
		        (int)missingInstalled.size(), missingInstalled[0].c_str());
	for (int i = 0; i < 2; i++) {
		if (missing[i].empty())
			continue;
		warning("%d files from CD %d not found (e.g. %s); pass --cd%d=PATH pointing at the disc or a copy of it",
		        (int)missing[i].size(), i + 1, missing[i][0].c_str(), i + 1);
	}
	return missingInstalled.empty() && missing[0].empty() && missing[1].empty();
}

void ResourceManager::scan() {
	_images.clear();
	_sounds.clear();

	Common::ArchiveMemberList members;
	SearchMan.listMembers(members);
	for (const Common::ArchiveMemberPtr &member : members) {
		const Common::String &name = member->getName();
		if (Common::hasSuffixIgnoreCase(name, ".bmp"))
			_images.push_back(name);
		else if (Common::hasSuffixIgnoreCase(name, ".wav"))
			_sounds.push_back(name);
	}

	debugC(1, "resources", "Found %d images and %d sounds", (int)_images.size(), (int)_sounds.size());
}

void ResourceManager::logInventory() const {
	std::map<Common::String, std::pair<int, int64>> byExtension;

	Common::ArchiveMemberList members;
	SearchMan.listMembers(members);
	for (const Common::ArchiveMemberPtr &member : members) {
		Common::String name = member->getName();
		size_t dot = name.rfind('.');
		size_t slash = name.rfind('/');
		Common::String ext = dot == Common::String::npos || (slash != Common::String::npos && dot < slash)
		                         ? "(none)"
		                         : Common::toUpper(name.substr(dot + 1));
		std::unique_ptr<Common::SeekableReadStream> stream = member->createReadStream();
		auto &entry = byExtension[ext];
		entry.first++;
		entry.second += stream ? stream->size() : 0;
	}

	info("Game directory contains %d files:", (int)members.size());
	for (const auto &entry : byExtension)
		info("  %-8s %5d files %12lld bytes", entry.first.c_str(), entry.second.first, (long long)entry.second.second);
}

bool ResourceManager::loadImage(const Common::String &name, Image::BitmapDecoder &decoder) const {
	Common::File file;
	if (!file.open(name)) {
		warning("Could not open image %s", name.c_str());
		return false;
	}
	if (!decoder.loadStream(file)) {
		warning("Could not decode image %s", name.c_str());
		return false;
	}
	return true;
}

std::unique_ptr<Audio::RewindableAudioStream> ResourceManager::loadSound(const Common::String &name) const {
	Common::File file;
	if (!file.open(name)) {
		warning("Could not open sound %s", name.c_str());
		return nullptr;
	}
	return Audio::makeWAVStream(file);
}

} // End of namespace STG
