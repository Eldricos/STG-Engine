/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "common/savefile.h"
#include "common/debug.h"

#include <filesystem>

namespace Common {

std::unique_ptr<InSaveFile> SaveFileManager::openForLoading(const String &name) const {
	return _saveDir.getChild(name).createReadStream();
}

std::unique_ptr<OutSaveFile> SaveFileManager::openForSaving(const String &name) {
	if (!_saveDir.isDirectory() && !_saveDir.createDirectory()) {
		warning("Could not create save directory %s", _saveDir.getPath().c_str());
		return nullptr;
	}
	return _saveDir.getChild(name).createWriteStream();
}

bool SaveFileManager::removeSavefile(const String &name) {
	std::error_code ec;
	return std::filesystem::remove(_saveDir.getChild(name).getNativePath(), ec);
}

bool SaveFileManager::exists(const String &name) const {
	return _saveDir.getChild(name).exists();
}

StringArray SaveFileManager::listSavefiles(const String &pattern) const {
	StringArray result;
	FSList files;
	_saveDir.getChildren(files, FSNode::kListFilesOnly);
	for (const FSNode &file : files) {
		if (matchString(file.getName(), pattern))
			result.push_back(file.getName());
	}
	return result;
}

} // End of namespace Common
