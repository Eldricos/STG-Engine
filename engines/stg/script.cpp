/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/script.h"

#include "common/debug.h"

namespace STG {

bool ScriptFile::load(Common::SeekableReadStream &stream) {
	_code.clear();
	_resourceNames.clear();

	if (stream.readUint32BE() != MKTAG('S', 'G', 'E', 'N')) {
		warning("ScriptFile: missing SGEN header");
		return false;
	}
	_version = stream.readUint16LE();
	stream.readUint16LE();
	uint32 size = stream.readUint32LE();
	uint32 codeSize = stream.readUint32LE();
	uint32 sectionSizes[kSectionCount];
	uint64 total = 40 + (uint64)codeSize;
	for (uint32 &sectionSize : sectionSizes) {
		sectionSize = stream.readUint32LE();
		total += sectionSize;
	}

	if ((int64)size + 12 != stream.size() || total > (uint64)stream.size() || (codeSize & 1)) {
		warning("ScriptFile: header sizes do not match a %lld byte file", (long long)stream.size());
		return false;
	}

	_code.resize(codeSize / 2);
	for (uint16 &word : _code)
		word = stream.readUint16LE();
	for (int i = 0; i < kSectionCount; i++) {
		_sections[i].resize(sectionSizes[i]);
		stream.read(_sections[i].data(), sectionSizes[i]);
	}

	std::vector<byte> names = stream.readAll();
	Common::String name;
	for (byte c : names) {
		if (c) {
			name += (char)c;
		} else if (!name.empty()) {
			_resourceNames.push_back(name);
			name.clear();
		}
	}
	if (!name.empty())
		_resourceNames.push_back(name);
	return true;
}

} // End of namespace STG
