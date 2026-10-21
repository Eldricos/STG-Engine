/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_FILE_H
#define COMMON_FILE_H

#include <memory>

#include "common/archive.h"
#include "common/fs.h"
#include "common/stream.h"

namespace Common {

/**
 * A read-only file looked up through an Archive, by default the global
 * SearchMan which holds the game directory.
 */
class File : public SeekableReadStream {
public:
	File() {}

	static bool exists(const String &filename);

	bool open(const String &filename);
	bool open(const String &filename, const Archive &archive);
	bool open(const FSNode &node);
	void close();
	bool isOpen() const { return (bool)_handle; }
	const String &getName() const { return _name; }

	bool eos() const override;
	bool err() const override;
	void clearErr() override;
	uint32 read(void *dataPtr, uint32 dataSize) override;
	int64 pos() const override;
	int64 size() const override;
	bool seek(int64 offset, SeekOrigin whence = kSeekSet) override;

private:
	std::unique_ptr<SeekableReadStream> _handle;
	String _name;
};

/** A file opened for writing at a host path. */
class DumpFile : public WriteStream {
public:
	bool open(const FSNode &node);
	void close();
	bool isOpen() const { return (bool)_handle; }

	bool err() const override;
	uint32 write(const void *dataPtr, uint32 dataSize) override;
	bool flush() override;

private:
	std::unique_ptr<WriteStream> _handle;
};

} // End of namespace Common

#endif
