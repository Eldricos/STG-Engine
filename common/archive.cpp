/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "common/archive.h"

#include <algorithm>
#include <set>

namespace Common {

namespace {

class FSNodeArchiveMember : public ArchiveMember {
public:
	FSNodeArchiveMember(const String &name, const FSNode &node) : _name(name), _node(node) {}

	String getName() const override { return _name; }
	std::unique_ptr<SeekableReadStream> createReadStream() const override { return _node.createReadStream(); }

private:
	String _name;
	FSNode _node;
};

} // End of anonymous namespace

int Archive::listMatchingMembers(ArchiveMemberList &list, const String &pattern) const {
	ArchiveMemberList all;
	listMembers(all);
	int count = 0;
	for (const ArchiveMemberPtr &member : all) {
		if (matchString(member->getName(), pattern)) {
			list.push_back(member);
			count++;
		}
	}
	return count;
}

FSDirectory::FSDirectory(const FSNode &node, int depth) : _node(node) {
	scan(node, "", depth);
}

String FSDirectory::normalize(const String &name) {
	String result = toLower(name);
	std::replace(result.begin(), result.end(), '\\', '/');
	return result;
}

void FSDirectory::scan(const FSNode &node, const String &prefix, int depth) {
	if (depth <= 0)
		return;

	FSList children;
	node.getChildren(children, FSNode::kListAll);
	for (const FSNode &child : children) {
		String name = prefix + child.getName();
		if (child.isDirectory()) {
			scan(child, name + "/", depth - 1);
		} else {
			String key = normalize(name);
			if (!_files.count(key))
				_files[key] = std::make_shared<FSNodeArchiveMember>(name, child);
		}
	}
}

bool FSDirectory::hasFile(const String &name) const {
	return _files.count(normalize(name)) != 0;
}

int FSDirectory::listMembers(ArchiveMemberList &list) const {
	for (const auto &entry : _files)
		list.push_back(entry.second);
	return (int)_files.size();
}

ArchiveMemberPtr FSDirectory::getMember(const String &name) const {
	auto it = _files.find(normalize(name));
	return it == _files.end() ? nullptr : it->second;
}

std::unique_ptr<SeekableReadStream> FSDirectory::createReadStreamForMember(const String &name) const {
	ArchiveMemberPtr member = getMember(name);
	return member ? member->createReadStream() : nullptr;
}

void SearchSet::add(const String &name, std::shared_ptr<Archive> archive, int priority) {
	remove(name);
	Node node = { priority, name, archive };
	// Keep sorted by descending priority; equal priorities keep insertion order.
	auto it = std::find_if(_list.begin(), _list.end(), [priority](const Node &n) { return n.priority < priority; });
	_list.insert(it, node);
}

void SearchSet::addDirectory(const String &name, const FSNode &directory, int priority, int depth) {
	if (!directory.isDirectory())
		return;
	add(name, std::make_shared<FSDirectory>(directory, depth), priority);
}

void SearchSet::remove(const String &name) {
	_list.erase(std::remove_if(_list.begin(), _list.end(), [&name](const Node &n) { return n.name == name; }), _list.end());
}

bool SearchSet::hasArchive(const String &name) const {
	return std::any_of(_list.begin(), _list.end(), [&name](const Node &n) { return n.name == name; });
}

void SearchSet::clear() {
	_list.clear();
}

bool SearchSet::hasFile(const String &name) const {
	for (const Node &node : _list) {
		if (node.archive->hasFile(name))
			return true;
	}
	return false;
}

int SearchSet::listMembers(ArchiveMemberList &list) const {
	return listMatchingMembers(list, "*");
}

int SearchSet::listMatchingMembers(ArchiveMemberList &list, const String &pattern) const {
	// A name in a higher priority archive hides the same name further down.
	std::set<String> seen;
	int count = 0;
	for (const Node &node : _list) {
		ArchiveMemberList members;
		node.archive->listMatchingMembers(members, pattern);
		for (const ArchiveMemberPtr &member : members) {
			if (seen.insert(toLower(member->getName())).second) {
				list.push_back(member);
				count++;
			}
		}
	}
	return count;
}

ArchiveMemberPtr SearchSet::getMember(const String &name) const {
	for (const Node &node : _list) {
		ArchiveMemberPtr member = node.archive->getMember(name);
		if (member)
			return member;
	}
	return nullptr;
}

std::unique_ptr<SeekableReadStream> SearchSet::createReadStreamForMember(const String &name) const {
	ArchiveMemberPtr member = getMember(name);
	return member ? member->createReadStream() : nullptr;
}

SearchSet &getSearchManager() {
	static SearchSet searchMan;
	return searchMan;
}

} // End of namespace Common
