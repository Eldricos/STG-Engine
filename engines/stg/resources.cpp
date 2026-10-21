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
