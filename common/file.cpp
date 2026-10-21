/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "common/file.h"

namespace Common {

bool File::exists(const String &filename) {
	return SearchMan.hasFile(filename);
}

bool File::open(const String &filename) {
	return open(filename, SearchMan);
}

bool File::open(const String &filename, const Archive &archive) {
	close();
	_handle = archive.createReadStreamForMember(filename);
	if (_handle)
		_name = filename;
	return isOpen();
}

bool File::open(const FSNode &node) {
	close();
	_handle = node.createReadStream();
	if (_handle)
		_name = node.getName();
	return isOpen();
}

void File::close() {
	_handle.reset();
	_name.clear();
}

bool File::eos() const {
	return _handle ? _handle->eos() : true;
}

bool File::err() const {
	return _handle ? _handle->err() : false;
}

void File::clearErr() {
	if (_handle)
		_handle->clearErr();
}

uint32 File::read(void *dataPtr, uint32 dataSize) {
	return _handle ? _handle->read(dataPtr, dataSize) : 0;
}

int64 File::pos() const {
	return _handle ? _handle->pos() : -1;
}

int64 File::size() const {
	return _handle ? _handle->size() : -1;
}

bool File::seek(int64 offset, SeekOrigin whence) {
	return _handle ? _handle->seek(offset, whence) : false;
}

bool DumpFile::open(const FSNode &node) {
	_handle = node.createWriteStream();
	return isOpen();
}

void DumpFile::close() {
	if (_handle)
		_handle->flush();
	_handle.reset();
}

bool DumpFile::err() const {
	return _handle ? _handle->err() : true;
}

uint32 DumpFile::write(const void *dataPtr, uint32 dataSize) {
	return _handle ? _handle->write(dataPtr, dataSize) : 0;
}

bool DumpFile::flush() {
	return _handle ? _handle->flush() : false;
}

} // End of namespace Common
