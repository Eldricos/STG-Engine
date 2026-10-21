/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_MD5_H
#define COMMON_MD5_H

#include "common/stream.h"
#include "common/str.h"
#include "common/types.h"

namespace Common {

/** MD5 of a memory buffer, as 32 lower-case hex digits. */
String computeMD5(const void *data, uint32 length);

/**
 * MD5 of a stream, as 32 lower-case hex digits. If length is non-zero only
 * the first length bytes are hashed, which is how game detection keeps
 * hashing large data files cheap.
 */
String computeStreamMD5(ReadStream &stream, uint32 length = 0);

} // End of namespace Common

#endif
