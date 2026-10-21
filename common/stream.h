/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_STREAM_H
#define COMMON_STREAM_H

#include <memory>
#include <vector>

#include "common/endian.h"
#include "common/str.h"
#include "common/types.h"

namespace Common {

enum SeekOrigin {
	kSeekSet,
	kSeekCur,
	kSeekEnd
};

class Stream {
public:
	virtual ~Stream() {}

	/** True if a read or write error occurred. */
	virtual bool err() const { return false; }
	virtual void clearErr() {}
};

class ReadStream : virtual public Stream {
public:
	/** Returns true once a read has hit the end of the stream. */
	virtual bool eos() const = 0;

	/** Read up to dataSize bytes; returns the number actually read. */
	virtual uint32 read(void *dataPtr, uint32 dataSize) = 0;

	byte readByte() {
		byte b = 0;
		read(&b, 1);
		return b;
	}
	int8 readSByte() { return (int8)readByte(); }

	uint16 readUint16LE() {
		byte b[2] = { 0, 0 };
		read(b, 2);
		return READ_LE_UINT16(b);
	}
	uint32 readUint32LE() {
		byte b[4] = { 0, 0, 0, 0 };
		read(b, 4);
		return READ_LE_UINT32(b);
	}
	uint16 readUint16BE() {
		byte b[2] = { 0, 0 };
		read(b, 2);
		return READ_BE_UINT16(b);
	}
	uint32 readUint32BE() {
		byte b[4] = { 0, 0, 0, 0 };
		read(b, 4);
		return READ_BE_UINT32(b);
	}
	int16 readSint16LE() { return (int16)readUint16LE(); }
	int32 readSint32LE() { return (int32)readUint32LE(); }
	int16 readSint16BE() { return (int16)readUint16BE(); }
	int32 readSint32BE() { return (int32)readUint32BE(); }

	/** Read a fixed-size, NUL-padded string. */
	String readFixedString(uint32 len);
};

class SeekableReadStream : virtual public ReadStream {
public:
	virtual int64 pos() const = 0;
	virtual int64 size() const = 0;
	virtual bool seek(int64 offset, SeekOrigin whence = kSeekSet) = 0;

	bool skip(uint32 offset) { return seek(offset, kSeekCur); }

	/** Read a line terminated by LF or CRLF, without the terminator. */
	String readLine();

	/** Read the rest of the stream into memory. */
	std::vector<byte> readAll();
};

class WriteStream : virtual public Stream {
public:
	virtual uint32 write(const void *dataPtr, uint32 dataSize) = 0;
	virtual bool flush() { return true; }

	void writeByte(byte value) { write(&value, 1); }
	void writeUint16LE(uint16 value) {
		byte b[2];
		WRITE_LE_UINT16(b, value);
		write(b, 2);
	}
	void writeUint32LE(uint32 value) {
		byte b[4];
		WRITE_LE_UINT32(b, value);
		write(b, 4);
	}
	void writeUint16BE(uint16 value) {
		byte b[2];
		WRITE_BE_UINT16(b, value);
		write(b, 2);
	}
	void writeUint32BE(uint32 value) {
		byte b[4];
		WRITE_BE_UINT32(b, value);
		write(b, 4);
	}
	void writeString(const String &str) { write(str.data(), (uint32)str.size()); }
};

/** A read stream over a memory buffer, which it may own. */
class MemoryReadStream : public SeekableReadStream {
public:
	MemoryReadStream(const byte *data, uint32 size, bool disposeMemory = false);
	explicit MemoryReadStream(std::vector<byte> &&data);
	~MemoryReadStream() override;

	bool eos() const override { return _eos; }
	void clearErr() override { _eos = false; }
	uint32 read(void *dataPtr, uint32 dataSize) override;
	int64 pos() const override { return _pos; }
	int64 size() const override { return _size; }
	bool seek(int64 offset, SeekOrigin whence = kSeekSet) override;

private:
	std::vector<byte> _owned;
	const byte *_data;
	uint32 _size;
	uint32 _pos = 0;
	bool _dispose;
	bool _eos = false;
};

/** A window onto part of another stream, e.g. one entry of an archive. */
class SubReadStream : public SeekableReadStream {
public:
	SubReadStream(std::shared_ptr<SeekableReadStream> parent, int64 begin, int64 end);

	bool eos() const override { return _eos; }
	bool err() const override { return _parent->err(); }
	void clearErr() override { _eos = false; _parent->clearErr(); }
	uint32 read(void *dataPtr, uint32 dataSize) override;
	int64 pos() const override { return _pos - _begin; }
	int64 size() const override { return _end - _begin; }
	bool seek(int64 offset, SeekOrigin whence = kSeekSet) override;

private:
	std::shared_ptr<SeekableReadStream> _parent;
	int64 _begin, _end, _pos;
	bool _eos = false;
};

/** A growable in-memory write stream. */
class MemoryWriteStreamDynamic : public WriteStream {
public:
	uint32 write(const void *dataPtr, uint32 dataSize) override;
	const std::vector<byte> &getData() const { return _data; }
	uint32 size() const { return (uint32)_data.size(); }

private:
	std::vector<byte> _data;
};

} // End of namespace Common

#endif
