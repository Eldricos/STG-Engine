/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/script.h"

#include "common/debug.h"
#include "engines/stg/opcodes.h"

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

	// AMARGOS2's GAME.BIN has 72 bytes of padding after what its header
	// counts; anything shorter than the header says is damaged.
	if ((int64)size + 12 > stream.size() || total > (uint64)size + 12 || (codeSize & 1)) {
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
	return parseProgram();
}

bool ScriptFile::parseProgram() {
	_objects.clear();
	const size_t size = _code.size();
	size_t p = 0;
	auto word = [&](size_t i) -> uint16 { return i < size ? _code[i] : 0x1000; };

	while (word(p) != kEndOfProgram) {
		if (p + 2 > size) {
			warning("ScriptFile: program runs past the end of the code");
			return false;
		}
		ScriptObject object;
		object.type = word(p);
		object.id = word(p + 1);
		p += 2;

		while (true) {
			uint16 header = word(p);
			if ((header & 0xF000) != kStateMarker) {
				warning("ScriptFile: expected a state at word %u, found %04x", (uint)p, header);
				return false;
			}
			ScriptState state;
			state.id = header & 0x0FFF;
			p++;

			while (word(p) != kEndOfState) {
				ScriptBlock block;
				block.event = word(p);
				block.nextState = (int16)word(p + 1);
				block.codeStart = (uint32)(p + 2);
				p += 2;
				while (word(p) != OP_END) {
					p += getInstructionLength(word(p));
					if (p >= size) {
						warning("ScriptFile: block at word %u has no end", block.codeStart);
						return false;
					}
				}
				p++;
				state.blocks.push_back(block);
			}
			p++;
			object.states.push_back(std::move(state));

			if (word(p) == kEndOfObject) {
				p++;
				break;
			}
		}
		_objects.push_back(std::move(object));
	}

	if (p + 1 != size) {
		warning("ScriptFile: program ends at word %u of %u", (uint)p + 1, (uint)size);
		return false;
	}
	return true;
}

} // End of namespace STG
