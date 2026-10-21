/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_ENDIAN_H
#define COMMON_ENDIAN_H

#include "common/types.h"

// Byte-wise accessors: independent of host endianness and alignment.

inline uint16 READ_LE_UINT16(const void *ptr) {
	const byte *b = (const byte *)ptr;
	return (uint16)(b[0] | (b[1] << 8));
}

inline uint32 READ_LE_UINT32(const void *ptr) {
	const byte *b = (const byte *)ptr;
	return (uint32)b[0] | ((uint32)b[1] << 8) | ((uint32)b[2] << 16) | ((uint32)b[3] << 24);
}

inline uint64 READ_LE_UINT64(const void *ptr) {
	const byte *b = (const byte *)ptr;
	return (uint64)READ_LE_UINT32(b) | ((uint64)READ_LE_UINT32(b + 4) << 32);
}

inline uint16 READ_BE_UINT16(const void *ptr) {
	const byte *b = (const byte *)ptr;
	return (uint16)((b[0] << 8) | b[1]);
}

inline uint32 READ_BE_UINT32(const void *ptr) {
	const byte *b = (const byte *)ptr;
	return ((uint32)b[0] << 24) | ((uint32)b[1] << 16) | ((uint32)b[2] << 8) | (uint32)b[3];
}

inline void WRITE_LE_UINT16(void *ptr, uint16 value) {
	byte *b = (byte *)ptr;
	b[0] = (byte)value;
	b[1] = (byte)(value >> 8);
}

inline void WRITE_LE_UINT32(void *ptr, uint32 value) {
	byte *b = (byte *)ptr;
	b[0] = (byte)value;
	b[1] = (byte)(value >> 8);
	b[2] = (byte)(value >> 16);
	b[3] = (byte)(value >> 24);
}

inline void WRITE_BE_UINT16(void *ptr, uint16 value) {
	byte *b = (byte *)ptr;
	b[0] = (byte)(value >> 8);
	b[1] = (byte)value;
}

inline void WRITE_BE_UINT32(void *ptr, uint32 value) {
	byte *b = (byte *)ptr;
	b[0] = (byte)(value >> 24);
	b[1] = (byte)(value >> 16);
	b[2] = (byte)(value >> 8);
	b[3] = (byte)value;
}

#define MKTAG(a0, a1, a2, a3) ((uint32)((a3) | ((a2) << 8) | ((a1) << 16) | ((a0) << 24)))

#endif
