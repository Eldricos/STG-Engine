/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_ZIP_H
#define COMMON_ZIP_H

#include <map>
#include <memory>
#include <vector>

#include "common/archive.h"
#include "common/fs.h"

namespace Common {

/**
 * Read-only access to a PKZIP archive.
 *
 * Supports stored and deflated members, which is what PKZIP 2.x and
 * later write. Members using other methods are listed but cannot be
 * opened; encrypted members likewise.
 */
class ZipArchive : public Archive {
public:
	enum Method {
		kMethodStored = 0,
		kMethodShrunk = 1,
		kMethodImploded = 6,
		kMethodDeflated = 8
	};

	struct Entry {
		String name;
		uint16 method = 0;
		uint16 flags = 0;
		uint32 crc = 0;
		uint32 compressedSize = 0;
		uint32 size = 0;
		uint32 localHeaderOffset = 0;
		/** Resolved lazily from the local header. */
		mutable int64 dataOffset = -1;

		bool isEncrypted() const { return flags & 1; }
	};

	/** Open a ZIP file. Returns nullptr if it isn't one. */
	static std::shared_ptr<ZipArchive> open(const FSNode &file);

	const FSNode &getFile() const { return _file; }
	/** All members in archive order. */
	const std::vector<Entry> &getEntries() const { return _entries; }
	const Entry *findEntry(const String &name) const;

	/** Decompress a member fully into memory. Returns false on error. */
	bool extract(const Entry &entry, std::vector<byte> &data) const;

	bool hasFile(const String &name) const override;
	int listMembers(ArchiveMemberList &list) const override;
	ArchiveMemberPtr getMember(const String &name) const override;
	std::unique_ptr<SeekableReadStream> createReadStreamForMember(const String &name) const override;

	static const char *getMethodName(uint16 method);

private:
	explicit ZipArchive(const FSNode &file) : _file(file) {}
	bool readCentralDirectory(SeekableReadStream &stream);
	bool resolveDataOffset(SeekableReadStream &stream, const Entry &entry) const;

	FSNode _file;
	/** Added to recorded offsets, for archives with data prepended. */
	int64 _offsetAdjust = 0;
	std::vector<Entry> _entries;
	std::map<String, size_t> _index; // normalised name -> entry
};

} // End of namespace Common

#endif
