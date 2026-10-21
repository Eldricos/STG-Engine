/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "common/fs.h"

#include <algorithm>
#include <fstream>

namespace fs = std::filesystem;

namespace Common {

namespace {

class FileReadStream : public SeekableReadStream {
public:
	FileReadStream(const fs::path &path) : _file(path, std::ios::binary) {
		if (_file) {
			_file.seekg(0, std::ios::end);
			_size = (int64)_file.tellg();
			_file.seekg(0, std::ios::beg);
		}
	}

	bool isOpen() const { return (bool)_file; }

	bool eos() const override { return _eos; }
	bool err() const override { return _err; }
	void clearErr() override {
		_eos = _err = false;
		_file.clear();
	}

	uint32 read(void *dataPtr, uint32 dataSize) override {
		_file.read((char *)dataPtr, dataSize);
		uint32 n = (uint32)_file.gcount();
		if (n < dataSize) {
			_eos = true;
			_file.clear();
		}
		return n;
	}

	int64 pos() const override { return (int64)const_cast<std::ifstream &>(_file).tellg(); }
	int64 size() const override { return _size; }

	bool seek(int64 offset, SeekOrigin whence) override {
		std::ios::seekdir dir = whence == kSeekCur ? std::ios::cur : whence == kSeekEnd ? std::ios::end : std::ios::beg;
		_file.clear();
		_file.seekg(offset, dir);
		if (!_file) {
			_file.clear();
			return false;
		}
		_eos = false;
		return true;
	}

private:
	std::ifstream _file;
	int64 _size = 0;
	bool _eos = false;
	bool _err = false;
};

class FileWriteStream : public WriteStream {
public:
	FileWriteStream(const fs::path &path) : _file(path, std::ios::binary | std::ios::trunc) {}

	bool isOpen() const { return (bool)_file; }
	bool err() const override { return !_file; }

	uint32 write(const void *dataPtr, uint32 dataSize) override {
		_file.write((const char *)dataPtr, dataSize);
		return _file ? dataSize : 0;
	}

	bool flush() override {
		_file.flush();
		return (bool)_file;
	}

private:
	std::ofstream _file;
};

} // End of anonymous namespace

FSNode::FSNode(const String &path) : _path(fs::u8path(path)), _valid(!path.empty()) {
}

bool FSNode::exists() const {
	std::error_code ec;
	return _valid && fs::exists(_path, ec);
}

bool FSNode::isDirectory() const {
	std::error_code ec;
	return _valid && fs::is_directory(_path, ec);
}

bool FSNode::isReadable() const {
	std::ifstream f(_path, std::ios::binary);
	return _valid && (isDirectory() || (bool)f);
}

String FSNode::getName() const {
	fs::path name = _path.filename();
	if (name.empty())
		name = _path.parent_path().filename();
	return name.u8string();
}

String FSNode::getPath() const {
	return _path.u8string();
}

FSNode FSNode::getParent() const {
	return FSNode(_path.parent_path());
}

FSNode FSNode::getChild(const String &name) const {
	fs::path exact = _path / fs::u8path(name);
	std::error_code ec;
	if (fs::exists(exact, ec))
		return FSNode(exact);

	for (const fs::directory_entry &entry : fs::directory_iterator(_path, ec)) {
		if (equalsIgnoreCase(entry.path().filename().u8string(), name))
			return FSNode(entry.path());
	}

	// Not found: return the exact path so it can be created.
	return FSNode(exact);
}

bool FSNode::getChildren(FSList &list, ListMode mode, bool hidden) const {
	std::error_code ec;
	if (!isDirectory())
		return false;

	for (const fs::directory_entry &entry : fs::directory_iterator(_path, ec)) {
		String name = entry.path().filename().u8string();
		if (!hidden && !name.empty() && name[0] == '.')
			continue;
		bool isDir = entry.is_directory(ec);
		if ((mode == kListFilesOnly && isDir) || (mode == kListDirectoriesOnly && !isDir))
			continue;
		list.push_back(FSNode(entry.path()));
	}
	std::sort(list.begin(), list.end());
	return !ec;
}

int64 FSNode::getFileSize() const {
	std::error_code ec;
	uintmax_t size = fs::file_size(_path, ec);
	return ec ? -1 : (int64)size;
}

std::unique_ptr<SeekableReadStream> FSNode::createReadStream() const {
	if (!_valid || isDirectory())
		return nullptr;
	std::unique_ptr<FileReadStream> stream(new FileReadStream(_path));
	if (!stream->isOpen())
		return nullptr;
	return stream;
}

std::unique_ptr<WriteStream> FSNode::createWriteStream() const {
	if (!_valid)
		return nullptr;
	std::unique_ptr<FileWriteStream> stream(new FileWriteStream(_path));
	if (!stream->isOpen())
		return nullptr;
	return stream;
}

bool FSNode::createDirectory() const {
	std::error_code ec;
	fs::create_directories(_path, ec);
	return isDirectory();
}

bool FSNode::operator<(const FSNode &node) const {
	bool dirA = isDirectory(), dirB = node.isDirectory();
	if (dirA != dirB)
		return dirA;
	return toLower(getName()) < toLower(node.getName());
}

} // End of namespace Common
