/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_SCRIPT_H
#define STG_SCRIPT_H

#include <vector>

#include "common/stream.h"
#include "common/str.h"

namespace STG {

/**
 * A compiled location script (GAME.BIN), container level only.
 *
 * Layout, from verid1/GAME.BIN and the AMARGOS1 header:
 *
 *   char[4]  "SGEN"
 *   uint16   22 (version)
 *   uint16   2
 *   uint32   file size - 12
 *   uint32   code size in bytes
 *   uint32   size of each of six data sections
 *   uint16   code[code size / 2]    bytecode for a stack machine;
 *                                   0x0002 n pushes n
 *   uint8    sections[6][...]       contents still unknown; section 1
 *                                   holds addresses inside sgens.exe and
 *                                   section 2 0x12345678 markers
 *   char[]   resource names, NUL separated (COMBADGE.BMP, KIRKHIT.OVR...)
 *
 * The header sizes account for every byte of the file. The instruction
 * set is not decoded yet.
 */
class ScriptFile {
public:
	static const int kSectionCount = 6;

	bool load(Common::SeekableReadStream &stream);

	uint16 getVersion() const { return _version; }
	const std::vector<uint16> &getCode() const { return _code; }
	const std::vector<byte> &getSection(int index) const { return _sections[index]; }
	const Common::StringArray &getResourceNames() const { return _resourceNames; }

private:
	uint16 _version = 0;
	std::vector<uint16> _code;
	std::vector<byte> _sections[kSectionCount];
	Common::StringArray _resourceNames;
};

} // End of namespace STG

#endif
