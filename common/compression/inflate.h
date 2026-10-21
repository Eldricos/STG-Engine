/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_COMPRESSION_INFLATE_H
#define COMMON_COMPRESSION_INFLATE_H

#include <cstddef>

#include "common/types.h"

namespace Common {

/**
 * Decompress raw DEFLATE data (RFC 1951, as stored in ZIP files) into a
 * buffer of known size. Returns false if the data is corrupt or does not
 * decompress to exactly dstLen bytes.
 */
bool inflateRaw(const byte *src, size_t srcLen, byte *dst, size_t dstLen);

/** CRC-32 as used by ZIP and PNG; pass the previous result to continue a running CRC. */
uint32 crc32(const byte *data, size_t len, uint32 crc = 0);

} // End of namespace Common

#endif
