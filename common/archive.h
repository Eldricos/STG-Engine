/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_ARCHIVE_H
#define COMMON_ARCHIVE_H

#include <map>
#include <memory>
#include <vector>

#include "common/fs.h"
#include "common/stream.h"
#include "common/str.h"

namespace Common {

class ArchiveMember {
public:
	virtual ~ArchiveMember() {}
	/** The member name, using '/' as separator for nested paths. */
	virtual String getName() const = 0;
	virtual std::unique_ptr<SeekableReadStream> createReadStream() const = 0;
};

typedef std::shared_ptr<ArchiveMember> ArchiveMemberPtr;
typedef std::vector<ArchiveMemberPtr> ArchiveMemberList;

/**
 * A collection of named files. This is implemented both by plain
 * directories and by the game's own container formats, so the engine can
 * look a resource up by name without caring where it actually lives.
 * All lookups are case-insensitive.
 */
class Archive {
public:
	virtual ~Archive() {}

	virtual bool hasFile(const String &name) const = 0;
	virtual int listMembers(ArchiveMemberList &list) const = 0;
	virtual ArchiveMemberPtr getMember(const String &name) const = 0;
	virtual std::unique_ptr<SeekableReadStream> createReadStreamForMember(const String &name) const = 0;

	/** List members matching a glob pattern such as "*.WAV". */
	virtual int listMatchingMembers(ArchiveMemberList &list, const String &pattern) const;
};

/** An archive backed by a host directory, optionally including subdirectories. */
class FSDirectory : public Archive {
public:
	/**
	 * @param node   The directory to expose.
	 * @param depth  How many levels to descend; 1 means only this directory.
	 */
	explicit FSDirectory(const FSNode &node, int depth = 1);

	const FSNode &getFSNode() const { return _node; }

	bool hasFile(const String &name) const override;
	int listMembers(ArchiveMemberList &list) const override;
	ArchiveMemberPtr getMember(const String &name) const override;
	std::unique_ptr<SeekableReadStream> createReadStreamForMember(const String &name) const override;

private:
	void scan(const FSNode &node, const String &prefix, int depth);
	static String normalize(const String &name);

	FSNode _node;
	// Lower-cased relative path -> member
	std::map<String, ArchiveMemberPtr> _files;
};

/**
 * A prioritized set of archives searched in turn. The global instance
 * (SearchMan) is what Common::File uses by default.
 */
class SearchSet : public Archive {
public:
	void add(const String &name, std::shared_ptr<Archive> archive, int priority = 0);
	void addDirectory(const String &name, const FSNode &directory, int priority = 0, int depth = 1);
	void remove(const String &name);
	bool hasArchive(const String &name) const;
	void clear();

	bool hasFile(const String &name) const override;
	int listMembers(ArchiveMemberList &list) const override;
	int listMatchingMembers(ArchiveMemberList &list, const String &pattern) const override;
	ArchiveMemberPtr getMember(const String &name) const override;
	std::unique_ptr<SeekableReadStream> createReadStreamForMember(const String &name) const override;

private:
	struct Node {
		int priority;
		String name;
		std::shared_ptr<Archive> archive;
	};
	std::vector<Node> _list;
};

SearchSet &getSearchManager();

} // End of namespace Common

#define SearchMan (Common::getSearchManager())

#endif
