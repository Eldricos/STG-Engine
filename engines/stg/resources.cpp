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
	_dataDirs.clear();
	_dataDirs.push_back(findDataDir(gameDir, "DATA"));
	SearchMan.addDirectory("game", gameDir, 0, 1);
	SearchMan.addDirectory("data", _dataDirs.back(), 10, 1);
	const Common::String cdPaths[2] = { cd1Path, cd2Path };
	for (int i = 0; i < 2; i++) {
		if (cdPaths[i].empty())
			continue;
		Common::FSNode cd(cdPaths[i]);
		if (!cd.isDirectory()) {
			warning("CD %d path '%s' is not a directory", i + 1, cdPaths[i].c_str());
			continue;
		}
		_dataDirs.push_back(findDataDir(cd, "GENS"));
		SearchMan.addDirectory(Common::format("cd%d", i + 1), _dataDirs.back(), 5 - i, 2);
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

void ResourceManager::openArchives() {
	_archives.clear();
	for (const DataFileLocation *f = kDataFiles; f->name; f++) {
		Common::String name = f->name;
		if (!Common::hasSuffixIgnoreCase(name, ".dat") && !Common::matchString(name, "MUSIC.?"))
			continue;

		// Installed copies first, then the discs, as in addSearchPaths().
		Common::FSNode file;
		for (const Common::FSNode &dir : _dataDirs) {
			file = dir.getChild(name);
			if (file.exists())
				break;
		}
		if (!file.exists())
			continue; // checkDataFiles() has already reported it
		std::shared_ptr<Common::ZipArchive> zip = Common::ZipArchive::open(file);
		if (!zip) {
			warning("%s is not a readable ZIP archive", name.c_str());
			continue;
		}
		debugC(1, "resources", "%s: %d members", name.c_str(), (int)zip->getEntries().size());
		_archives[Common::toUpper(name)] = zip;
	}

	// Shared resources are visible to everything by name.
	int priority = -1;
	for (const char *name : { "COMMON.DAT", "OPTIONS.DAT" }) {
		std::shared_ptr<Common::ZipArchive> zip = getArchive(name);
		if (zip)
			SearchMan.add(Common::String("zip:") + name, zip, priority--);
	}
	info("Opened %d data archives", (int)_archives.size());
}

std::shared_ptr<Common::ZipArchive> ResourceManager::getArchive(const Common::String &name) const {
	auto it = _archives.find(Common::toUpper(name));
	return it == _archives.end() ? nullptr : it->second;
}

void ResourceManager::scan() {
	_images.clear();
	_sounds.clear();

	auto add = [this](const Common::String &archive, const Common::String &name) {
		if (Common::hasSuffixIgnoreCase(name, ".bmp") || Common::hasSuffixIgnoreCase(name, ".ovr") ||
		    Common::hasSuffixIgnoreCase(name, ".gs2"))
			_images.push_back({ archive, name });
		else if (Common::hasSuffixIgnoreCase(name, ".wav"))
			_sounds.push_back({ archive, name });
	};

	// Archives in name order, members in archive order.
	for (const auto &entry : _archives) {
		for (const Common::ZipArchive::Entry &member : entry.second->getEntries())
			add(entry.first, member.name);
	}

	// Loose files in the data directories, if any.
	for (const Common::FSNode &dir : _dataDirs) {
		Common::FSDirectory files(dir, 2);
		Common::ArchiveMemberList members;
		files.listMembers(members);
		for (const Common::ArchiveMemberPtr &member : members)
			add("", member->getName());
	}

	debugC(1, "resources", "Found %d images and %d sounds", (int)_images.size(), (int)_sounds.size());
}

void ResourceManager::logInventory() const {
	for (const auto &entry : _archives) {
		std::map<Common::String, int> byExtension;
		for (const Common::ZipArchive::Entry &member : entry.second->getEntries()) {
			size_t dot = member.name.rfind('.');
			byExtension[dot == Common::String::npos ? "?" : Common::toUpper(member.name.substr(dot + 1))]++;
		}
		Common::String summary;
		for (const auto &ext : byExtension)
			summary += Common::format(" %d %s,", ext.second, ext.first.c_str());
		summary.pop_back();
		info("  %-12s%s", entry.first.c_str(), summary.c_str());
	}
}

bool ResourceManager::hasResource(const ResourceRef &ref) const {
	if (ref.archive.empty())
		return SearchMan.hasFile(ref.name);
	std::shared_ptr<Common::ZipArchive> zip = getArchive(ref.archive);
	return zip && zip->hasFile(ref.name);
}

std::unique_ptr<Common::SeekableReadStream> ResourceManager::open(const ResourceRef &ref) const {
	if (ref.archive.empty())
		return SearchMan.createReadStreamForMember(ref.name);
	std::shared_ptr<Common::ZipArchive> zip = getArchive(ref.archive);
	return zip ? zip->createReadStreamForMember(ref.name) : nullptr;
}

bool ResourceManager::loadImage(const ResourceRef &ref, Image::BitmapDecoder &decoder) const {
	std::unique_ptr<Common::SeekableReadStream> stream = open(ref);
	if (!stream) {
		warning("Could not open image %s", ref.toString().c_str());
		return false;
	}
	if (!decoder.loadStream(*stream)) {
		warning("Could not decode image %s", ref.toString().c_str());
		return false;
	}
	return true;
}

bool ResourceManager::loadArchivePalette(const Common::String &archive, Graphics::Palette &palette) const {
	for (const ResourceRef &ref : _images) {
		if (ref.archive != archive || !Common::hasSuffixIgnoreCase(ref.name, ".bmp"))
			continue;
		Image::BitmapDecoder decoder;
		if (loadImage(ref, decoder) && decoder.hasPalette()) {
			palette = decoder.getPalette();
			return true;
		}
	}
	return false;
}

std::unique_ptr<Audio::RewindableAudioStream> ResourceManager::loadSound(const ResourceRef &ref) const {
	std::unique_ptr<Common::SeekableReadStream> stream = open(ref);
	if (!stream) {
		warning("Could not open sound %s", ref.toString().c_str());
		return nullptr;
	}
	return Audio::makeWAVStream(*stream);
}

Common::String ResourceManager::loadSubtitle(const ResourceRef &soundRef) const {
	size_t dot = soundRef.name.rfind('.');
	if (dot == Common::String::npos)
		return Common::String();
	// Archive lookups ignore case, so ".TXT" matches either spelling.
	Common::String base = soundRef.name.substr(0, dot);
	ResourceRef textRef = { soundRef.archive, base + ".TXT" };
	std::unique_ptr<Common::SeekableReadStream> stream = open(textRef);
	if (!stream)
		return Common::String();

	Common::String text;
	while (!stream->eos()) {
		Common::String line = Common::trim(stream->readLine());
		if (!line.empty())
			text += (text.empty() ? "" : " ") + line;
	}
	return text;
}

} // End of namespace STG
