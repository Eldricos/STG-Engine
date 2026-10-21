/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

// Straightforward implementation of RFC 1321.

#include "common/md5.h"
#include "common/endian.h"

#include <cstring>

namespace Common {

namespace {

struct MD5Context {
	uint32 state[4];
	uint64 length;
	byte buffer[64];
	uint32 bufferLen;
};

const uint32 kSineTable[64] = {
	0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
	0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
	0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
	0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
	0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
	0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
	0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
	0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

const uint32 kShifts[64] = {
	7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
	5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
	4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
	6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};

inline uint32 rotl(uint32 x, uint32 c) {
	return (x << c) | (x >> (32 - c));
}

void md5Transform(uint32 state[4], const byte block[64]) {
	uint32 m[16];
	for (int i = 0; i < 16; i++)
		m[i] = READ_LE_UINT32(block + i * 4);

	uint32 a = state[0], b = state[1], c = state[2], d = state[3];
	for (uint32 i = 0; i < 64; i++) {
		uint32 f, g;
		if (i < 16) {
			f = (b & c) | (~b & d);
			g = i;
		} else if (i < 32) {
			f = (d & b) | (~d & c);
			g = (5 * i + 1) % 16;
		} else if (i < 48) {
			f = b ^ c ^ d;
			g = (3 * i + 5) % 16;
		} else {
			f = c ^ (b | ~d);
			g = (7 * i) % 16;
		}
		uint32 tmp = d;
		d = c;
		c = b;
		b = b + rotl(a + f + kSineTable[i] + m[g], kShifts[i]);
		a = tmp;
	}

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
}

void md5Init(MD5Context &ctx) {
	ctx.state[0] = 0x67452301;
	ctx.state[1] = 0xefcdab89;
	ctx.state[2] = 0x98badcfe;
	ctx.state[3] = 0x10325476;
	ctx.length = 0;
	ctx.bufferLen = 0;
}

void md5Update(MD5Context &ctx, const byte *data, uint32 len) {
	ctx.length += len;
	while (len > 0) {
		uint32 n = 64 - ctx.bufferLen;
		if (n > len)
			n = len;
		memcpy(ctx.buffer + ctx.bufferLen, data, n);
		ctx.bufferLen += n;
		data += n;
		len -= n;
		if (ctx.bufferLen == 64) {
			md5Transform(ctx.state, ctx.buffer);
			ctx.bufferLen = 0;
		}
	}
}

String md5Final(MD5Context &ctx) {
	uint64 bitLength = ctx.length * 8;
	byte pad = 0x80;
	md5Update(ctx, &pad, 1);
	byte zero = 0;
	while (ctx.bufferLen != 56)
		md5Update(ctx, &zero, 1);
	byte lenBytes[8];
	for (int i = 0; i < 8; i++)
		lenBytes[i] = (byte)(bitLength >> (8 * i));
	md5Update(ctx, lenBytes, 8);

	static const char hex[] = "0123456789abcdef";
	String result;
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			byte b = (byte)(ctx.state[i] >> (8 * j));
			result += hex[b >> 4];
			result += hex[b & 15];
		}
	}
	return result;
}

} // End of anonymous namespace

String computeMD5(const void *data, uint32 length) {
	MD5Context ctx;
	md5Init(ctx);
	md5Update(ctx, (const byte *)data, length);
	return md5Final(ctx);
}

String computeStreamMD5(ReadStream &stream, uint32 length) {
	MD5Context ctx;
	md5Init(ctx);
	byte buf[4096];
	uint32 remaining = length ? length : 0xFFFFFFFF;
	while (remaining > 0) {
		uint32 n = stream.read(buf, remaining < sizeof(buf) ? remaining : (uint32)sizeof(buf));
		if (n == 0)
			break;
		md5Update(ctx, buf, n);
		remaining -= n;
	}
	return md5Final(ctx);
}

} // End of namespace Common
