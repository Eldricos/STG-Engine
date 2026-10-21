/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "common/stream.h"

#include <algorithm>
#include <cstring>

namespace Common {

String ReadStream::readFixedString(uint32 len) {
	std::vector<char> buf(len);
	read(buf.data(), len);
	size_t n = 0;
	while (n < len && buf[n])
		n++;
	return String(buf.data(), n);
}

String SeekableReadStream::readLine() {
	String line;
	while (true) {
		byte c;
		if (read(&c, 1) != 1)
			break;
		if (c == '\n')
			break;
		if (c == '\r') {
			// Swallow the LF of a CRLF pair, otherwise put the byte back.
			byte next;
			if (read(&next, 1) == 1 && next != '\n')
				seek(-1, kSeekCur);
			break;
		}
		line += (char)c;
	}
	return line;
}

std::vector<byte> SeekableReadStream::readAll() {
	int64 remaining = size() - pos();
	std::vector<byte> data(remaining > 0 ? (size_t)remaining : 0);
	if (!data.empty())
		data.resize(read(data.data(), (uint32)data.size()));
	return data;
}

MemoryReadStream::MemoryReadStream(const byte *data, uint32 size, bool disposeMemory)
	: _data(data), _size(size), _dispose(disposeMemory) {
}

MemoryReadStream::MemoryReadStream(std::vector<byte> &&data)
	: _owned(std::move(data)), _data(nullptr), _size(0), _dispose(false) {
	_data = _owned.data();
	_size = (uint32)_owned.size();
}

MemoryReadStream::~MemoryReadStream() {
	if (_dispose)
		free(const_cast<byte *>(_data));
}

uint32 MemoryReadStream::read(void *dataPtr, uint32 dataSize) {
	if (dataSize > _size - _pos) {
		dataSize = _size - _pos;
		_eos = true;
	}
	if (dataSize)
		memcpy(dataPtr, _data + _pos, dataSize);
	_pos += dataSize;
	return dataSize;
}

bool MemoryReadStream::seek(int64 offset, SeekOrigin whence) {
	int64 newPos;
	switch (whence) {
	case kSeekCur:
		newPos = _pos + offset;
		break;
	case kSeekEnd:
		newPos = _size + offset;
		break;
	case kSeekSet:
	default:
		newPos = offset;
		break;
	}
	if (newPos < 0 || newPos > _size)
		return false;
	_pos = (uint32)newPos;
	_eos = false;
	return true;
}

SubReadStream::SubReadStream(std::shared_ptr<SeekableReadStream> parent, int64 begin, int64 end)
	: _parent(parent), _begin(begin), _end(end), _pos(begin) {
}

uint32 SubReadStream::read(void *dataPtr, uint32 dataSize) {
	if (_pos + dataSize > _end) {
		dataSize = (uint32)(_end - _pos);
		_eos = true;
	}
	if (!dataSize)
		return 0;
	_parent->seek(_pos);
	uint32 n = _parent->read(dataPtr, dataSize);
	_pos += n;
	return n;
}

bool SubReadStream::seek(int64 offset, SeekOrigin whence) {
	int64 newPos;
	switch (whence) {
	case kSeekCur:
		newPos = _pos + offset;
		break;
	case kSeekEnd:
		newPos = _end + offset;
		break;
	case kSeekSet:
	default:
		newPos = _begin + offset;
		break;
	}
	if (newPos < _begin || newPos > _end)
		return false;
	_pos = newPos;
	_eos = false;
	return true;
}

uint32 MemoryWriteStreamDynamic::write(const void *dataPtr, uint32 dataSize) {
	const byte *p = (const byte *)dataPtr;
	_data.insert(_data.end(), p, p + dataSize);
	return dataSize;
}

} // End of namespace Common
