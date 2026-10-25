/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "common/zip.h"

#include <algorithm>

#include "common/compression/inflate.h"
#include "common/debug.h"

namespace Common {

namespace {

const uint32 kLocalHeaderSig = 0x04034b50;
const uint32 kCentralHeaderSig = 0x02014b50;
const uint32 kEndOfCentralDirSig = 0x06054b50;
const uint32 kEndOfCentralDirSize = 22;

String normalize(const String &name) {
	String result = toLower(name);
	std::replace(result.begin(), result.end(), '\\', '/');
	return result;
}

class ZipMember : public ArchiveMember {
public:
	ZipMember(const ZipArchive *archive, const String &name) : _archive(archive), _name(name) {}

	String getName() const override { return _name; }
	std::unique_ptr<SeekableReadStream> createReadStream() const override {
		return _archive->createReadStreamForMember(_name);
	}

private:
	const ZipArchive *_archive;
	String _name;
};

} // End of anonymous namespace

const char *ZipArchive::getMethodName(uint16 method) {
	switch (method) {
	case kMethodStored:
		return "stored";
	case kMethodShrunk:
		return "shrunk";
	case 2:
	case 3:
	case 4:
	case 5:
		return "reduced";
	case kMethodImploded:
		return "imploded";
	case kMethodDeflated:
		return "deflated";
	case 9:
		return "deflate64";
	case 12:
		return "bzip2";
	default:
		return "unknown";
	}
}

std::shared_ptr<ZipArchive> ZipArchive::open(const FSNode &file) {
	std::unique_ptr<SeekableReadStream> stream = file.createReadStream();
	if (!stream)
		return nullptr;
	std::shared_ptr<ZipArchive> archive(new ZipArchive(file));
	if (!archive->readCentralDirectory(*stream))
		return nullptr;
	return archive;
}

bool ZipArchive::readCentralDirectory(SeekableReadStream &stream) {
	int64 size = stream.size();
	if (size < kEndOfCentralDirSize)
		return false;

	// The end record is at the very end, unless followed by a comment of up to 64KB.
	int64 searchStart = std::max<int64>(0, size - kEndOfCentralDirSize - 0xFFFF);
	stream.seek(searchStart);
	std::vector<byte> tail = stream.readAll();
	int64 eocd = -1;
	for (int64 i = (int64)tail.size() - kEndOfCentralDirSize; i >= 0; i--) {
		if (READ_LE_UINT32(&tail[i]) == kEndOfCentralDirSig) {
			eocd = i;
			break;
		}
	}
	if (eocd < 0)
		return false;

	const byte *e = &tail[eocd];
	uint16 entryCount = READ_LE_UINT16(e + 10);
	uint32 cdSize = READ_LE_UINT32(e + 12);
	uint32 cdOffset = READ_LE_UINT32(e + 16);

	// If data was prepended (e.g. a self-extractor stub), recorded offsets are
	// short by its length.
	int64 eocdPos = searchStart + eocd;
	_offsetAdjust = eocdPos - ((int64)cdOffset + cdSize);
	if (_offsetAdjust < 0)
		return false;

	stream.seek(cdOffset + _offsetAdjust);
	for (uint32 i = 0; i < entryCount; i++) {
		byte h[46];
		if (stream.read(h, sizeof(h)) != sizeof(h) || READ_LE_UINT32(h) != kCentralHeaderSig) {
			warning("ZipArchive: %s: corrupt central directory at entry %u", _file.getName().c_str(), i);
			return false;
		}
		Entry entry;
		entry.flags = READ_LE_UINT16(h + 8);
		entry.method = READ_LE_UINT16(h + 10);
		entry.crc = READ_LE_UINT32(h + 16);
		entry.compressedSize = READ_LE_UINT32(h + 20);
		entry.size = READ_LE_UINT32(h + 24);
		uint16 nameLen = READ_LE_UINT16(h + 28);
		uint16 extraLen = READ_LE_UINT16(h + 30);
		uint16 commentLen = READ_LE_UINT16(h + 32);
		entry.localHeaderOffset = READ_LE_UINT32(h + 42);
		entry.name = stream.readFixedString(nameLen);
		std::replace(entry.name.begin(), entry.name.end(), '\\', '/');
		stream.skip(extraLen + commentLen);

		if (entry.name.empty() || entry.name.back() == '/')
			continue; // directory entry
		String key = normalize(entry.name);
		if (_index.count(key))
			continue; // keep the first of any duplicates
		_index[key] = _entries.size();
		_entries.push_back(entry);
	}
	return true;
}

bool ZipArchive::resolveDataOffset(SeekableReadStream &stream, const Entry &entry) const {
	if (entry.dataOffset >= 0)
		return true;
	byte h[30];
	if (!stream.seek(entry.localHeaderOffset + _offsetAdjust) || stream.read(h, sizeof(h)) != sizeof(h) ||
	    READ_LE_UINT32(h) != kLocalHeaderSig) {
		warning("ZipArchive: %s: bad local header for %s", _file.getName().c_str(), entry.name.c_str());
		return false;
	}
	// The local name/extra lengths can differ from the central directory's.
	entry.dataOffset = entry.localHeaderOffset + _offsetAdjust + 30 + READ_LE_UINT16(h + 26) + READ_LE_UINT16(h + 28);
	return true;
}

const ZipArchive::Entry *ZipArchive::findEntry(const String &name) const {
	auto it = _index.find(normalize(name));
	return it == _index.end() ? nullptr : &_entries[it->second];
}

bool ZipArchive::extract(const Entry &entry, std::vector<byte> &data) const {
	if (entry.isEncrypted()) {
		warning("ZipArchive: %s is encrypted", entry.name.c_str());
		return false;
	}
	if (entry.method != kMethodStored && entry.method != kMethodDeflated) {
		warning("ZipArchive: %s uses unsupported method %u (%s)", entry.name.c_str(), entry.method,
		        getMethodName(entry.method));
		return false;
	}

	std::unique_ptr<SeekableReadStream> stream = _file.createReadStream();
	if (!stream || !resolveDataOffset(*stream, entry))
		return false;

	std::vector<byte> compressed(entry.compressedSize);
	stream->seek(entry.dataOffset);
	if (stream->read(compressed.data(), entry.compressedSize) != entry.compressedSize) {
		warning("ZipArchive: %s is truncated", entry.name.c_str());
		return false;
	}

	if (entry.method == kMethodStored) {
		data = std::move(compressed);
	} else {
		data.resize(entry.size);
		if (!inflateRaw(compressed.data(), compressed.size(), data.data(), data.size())) {
			warning("ZipArchive: %s failed to decompress", entry.name.c_str());
			return false;
		}
	}

	if (crc32(data.data(), data.size()) != entry.crc) {
		warning("ZipArchive: %s failed its CRC check", entry.name.c_str());
		return false;
	}
	return true;
}

bool ZipArchive::hasFile(const String &name) const {
	return findEntry(name) != nullptr;
}

int ZipArchive::listMembers(ArchiveMemberList &list) const {
	for (const Entry &entry : _entries)
		list.push_back(std::make_shared<ZipMember>(this, entry.name));
	return (int)_entries.size();
}

ArchiveMemberPtr ZipArchive::getMember(const String &name) const {
	const Entry *entry = findEntry(name);
	return entry ? std::make_shared<ZipMember>(this, entry->name) : nullptr;
}

std::unique_ptr<SeekableReadStream> ZipArchive::createReadStreamForMember(const String &name) const {
	const Entry *entry = findEntry(name);
	if (!entry)
		return nullptr;

	if (entry->method == kMethodStored && !entry->isEncrypted()) {
		// Stream straight from the file. Each member gets its own file handle so
		// streams can be read from different threads (e.g. audio).
		std::shared_ptr<SeekableReadStream> file(_file.createReadStream().release());
		if (!file || !resolveDataOffset(*file, *entry))
			return nullptr;
		return std::unique_ptr<SeekableReadStream>(
			new SubReadStream(file, entry->dataOffset, entry->dataOffset + entry->size));
	}

	std::vector<byte> data;
	if (!extract(*entry, data))
		return nullptr;
	return std::unique_ptr<SeekableReadStream>(new MemoryReadStream(std::move(data)));
}

} // End of namespace Common
