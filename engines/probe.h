/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef ENGINES_PROBE_H
#define ENGINES_PROBE_H

#include "common/fs.h"
#include "common/str.h"

/**
 * Reverse engineering aid: identify a file's format from its contents.
 *
 * Known formats (RIFF WAVE/AVI, BMP, executables, ...) are described;
 * for AVI the stream codecs are listed. ZIP archives are summarised by
 * extension, with one member of each type probed in turn; listMembers
 * also lists every member. Unknown files get a hex dump of
 * their first dumpBytes bytes and the leading little-endian 32-bit
 * values, which is usually enough to spot a header and offset table.
 */
Common::String probeFile(const Common::FSNode &file, const Common::String &name, uint32 dumpBytes,
                         bool listMembers = false);

#endif
