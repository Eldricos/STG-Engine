/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_SAVEFILE_H
#define COMMON_SAVEFILE_H

#include <memory>

#include "common/fs.h"
#include "common/stream.h"
#include "common/str.h"

namespace Common {

typedef SeekableReadStream InSaveFile;
typedef WriteStream OutSaveFile;

/**
 * Manages saved games in the per-user save directory provided by the
 * backend, keeping them out of the (possibly read-only) game directory.
 */
class SaveFileManager {
public:
	explicit SaveFileManager(const FSNode &saveDir) : _saveDir(saveDir) {}

	const FSNode &getSavePath() const { return _saveDir; }
	void setSavePath(const FSNode &saveDir) { _saveDir = saveDir; }

	std::unique_ptr<InSaveFile> openForLoading(const String &name) const;
	std::unique_ptr<OutSaveFile> openForSaving(const String &name);
	bool removeSavefile(const String &name);
	bool exists(const String &name) const;

	/** Names of save files matching a glob pattern, e.g. "stg.*". */
	StringArray listSavefiles(const String &pattern) const;

private:
	FSNode _saveDir;
};

} // End of namespace Common

#endif
