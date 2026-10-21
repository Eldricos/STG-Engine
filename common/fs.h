/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_FS_H
#define COMMON_FS_H

#include <filesystem>
#include <memory>
#include <vector>

#include "common/stream.h"
#include "common/str.h"

namespace Common {

class FSNode;
typedef std::vector<FSNode> FSList;

/**
 * A node in the host file system. Engines never deal with host paths
 * directly; they go through FSNode, FSDirectory and the SearchMan so the
 * case-insensitive lookups the original Windows game relied on keep working
 * on case-sensitive file systems.
 */
class FSNode {
public:
	enum ListMode {
		kListFilesOnly,
		kListDirectoriesOnly,
		kListAll
	};

	FSNode() {}
	explicit FSNode(const String &path);
	explicit FSNode(const std::filesystem::path &path) : _path(path), _valid(true) {}

	bool isValid() const { return _valid; }
	bool exists() const;
	bool isDirectory() const;
	bool isReadable() const;

	/** The last path component, e.g. "GAME.EXE". */
	String getName() const;
	/** The full host path as UTF-8. */
	String getPath() const;
	const std::filesystem::path &getNativePath() const { return _path; }

	FSNode getParent() const;

	/**
	 * Return the named child. If no child with that exact name exists,
	 * a case-insensitive match is looked for.
	 */
	FSNode getChild(const String &name) const;

	bool getChildren(FSList &list, ListMode mode = kListAll, bool hidden = false) const;

	int64 getFileSize() const;

	std::unique_ptr<SeekableReadStream> createReadStream() const;
	std::unique_ptr<WriteStream> createWriteStream() const;

	/** Create this directory and any missing parents. */
	bool createDirectory() const;

	bool operator<(const FSNode &node) const;

private:
	std::filesystem::path _path;
	bool _valid = false;
};

} // End of namespace Common

#endif
