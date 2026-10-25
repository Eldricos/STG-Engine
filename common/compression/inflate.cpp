/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

// A compact DEFLATE decoder following RFC 1951, structured like Mark
// Adler's reference "puff" decoder: canonical Huffman decoding one bit at a
// time. Simple rather than fast, which is fine for game resources.

#include "common/compression/inflate.h"

#include <cstring>

namespace Common {

namespace {

const int kMaxBits = 15;
const int kMaxLengthCodes = 286;
const int kMaxDistCodes = 30;
const int kFixedLengthCodes = 288;

struct Huffman {
	short count[kMaxBits + 1]; // number of codes of each length
	short symbol[kFixedLengthCodes]; // symbols ordered by code
};

class Inflater {
public:
	Inflater(const byte *src, size_t srcLen, byte *dst, size_t dstLen)
		: _in(src), _inLen(srcLen), _out(dst), _outLen(dstLen) {}

	bool run();
	size_t outputSize() const { return _outPos; }

private:
	// Read n bits, least significant first. Sets _error past the end of input.
	int bits(int n) {
		uint32 val = _bitBuf;
		while (_bitCount < n) {
			if (_inPos >= _inLen) {
				_error = true;
				return 0;
			}
			val |= (uint32)_in[_inPos++] << _bitCount;
			_bitCount += 8;
		}
		_bitBuf = val >> n;
		_bitCount -= n;
		return (int)(val & ((1u << n) - 1));
	}

	bool stored();
	bool codes(const Huffman &lencode, const Huffman &distcode);
	bool fixed();
	bool dynamic();
	int decode(const Huffman &h);

	static bool construct(Huffman &h, const short *length, int n);

	const byte *_in;
	size_t _inLen;
	size_t _inPos = 0;
	byte *_out;
	size_t _outLen;
	size_t _outPos = 0;
	uint32 _bitBuf = 0;
	int _bitCount = 0;
	bool _error = false;
};

bool Inflater::stored() {
	// Discard the rest of the current byte.
	_bitBuf = 0;
	_bitCount = 0;
	if (_inPos + 4 > _inLen)
		return false;
	uint32 len = _in[_inPos] | (_in[_inPos + 1] << 8);
	uint32 nlen = _in[_inPos + 2] | (_in[_inPos + 3] << 8);
	_inPos += 4;
	if (len != (~nlen & 0xFFFF))
		return false;
	if (_inPos + len > _inLen || _outPos + len > _outLen)
		return false;
	memcpy(_out + _outPos, _in + _inPos, len);
	_inPos += len;
	_outPos += len;
	return true;
}

int Inflater::decode(const Huffman &h) {
	int code = 0;  // bits read so far
	int first = 0; // first code of the current length
	int index = 0; // index of the first code of the current length in symbol[]
	for (int len = 1; len <= kMaxBits; len++) {
		code |= bits(1);
		if (_error)
			return -1;
		int count = h.count[len];
		if (code - count < first)
			return h.symbol[index + (code - first)];
		index += count;
		first += count;
		first <<= 1;
		code <<= 1;
	}
	return -1;
}

bool Inflater::construct(Huffman &h, const short *length, int n) {
	memset(h.count, 0, sizeof(h.count));
	for (int sym = 0; sym < n; sym++)
		h.count[length[sym]]++;
	if (h.count[0] == n)
		return true; // no codes: valid, but any decode will fail

	// Reject over-subscribed codes. Incomplete codes are allowed, as zlib does.
	int left = 1;
	for (int len = 1; len <= kMaxBits; len++) {
		left <<= 1;
		left -= h.count[len];
		if (left < 0)
			return false;
	}

	short offs[kMaxBits + 1];
	offs[1] = 0;
	for (int len = 1; len < kMaxBits; len++)
		offs[len + 1] = offs[len] + h.count[len];
	for (int sym = 0; sym < n; sym++) {
		if (length[sym] != 0)
			h.symbol[offs[length[sym]]++] = (short)sym;
	}
	return true;
}

bool Inflater::codes(const Huffman &lencode, const Huffman &distcode) {
	static const short lbase[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
	                                 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
	static const short lext[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
	                                3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
	static const short dbase[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
	                                 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
	                                 6145, 8193, 12289, 16385, 24577 };
	static const short dext[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
	                                7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

	while (true) {
		int symbol = decode(lencode);
		if (symbol < 0)
			return false;
		if (symbol < 256) {
			if (_outPos >= _outLen)
				return false;
			_out[_outPos++] = (byte)symbol;
		} else if (symbol == 256) {
			return true;
		} else {
			symbol -= 257;
			if (symbol >= 29)
				return false;
			int len = lbase[symbol] + bits(lext[symbol]);

			symbol = decode(distcode);
			if (symbol < 0 || symbol >= 30)
				return false;
			size_t dist = (size_t)(dbase[symbol] + bits(dext[symbol]));
			if (_error || dist > _outPos || _outPos + len > _outLen)
				return false;

			// Byte by byte: the source may overlap the destination.
			for (int i = 0; i < len; i++, _outPos++)
				_out[_outPos] = _out[_outPos - dist];
		}
	}
}

bool Inflater::fixed() {
	// Built once; static initialisation is thread safe.
	struct FixedTables {
		Huffman lencode, distcode;
		FixedTables() {
			short lengths[kFixedLengthCodes];
			int sym = 0;
			for (; sym < 144; sym++)
				lengths[sym] = 8;
			for (; sym < 256; sym++)
				lengths[sym] = 9;
			for (; sym < 280; sym++)
				lengths[sym] = 7;
			for (; sym < kFixedLengthCodes; sym++)
				lengths[sym] = 8;
			construct(lencode, lengths, kFixedLengthCodes);
			for (sym = 0; sym < kMaxDistCodes; sym++)
				lengths[sym] = 5;
			construct(distcode, lengths, kMaxDistCodes);
		}
	};
	static const FixedTables tables;
	return codes(tables.lencode, tables.distcode);
}

bool Inflater::dynamic() {
	static const short order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

	int nlen = bits(5) + 257;
	int ndist = bits(5) + 1;
	int ncode = bits(4) + 4;
	if (_error || nlen > kMaxLengthCodes || ndist > kMaxDistCodes)
		return false;

	short lengths[kMaxLengthCodes + kMaxDistCodes];
	int index;
	for (index = 0; index < ncode; index++)
		lengths[order[index]] = (short)bits(3);
	for (; index < 19; index++)
		lengths[order[index]] = 0;

	Huffman lencode, distcode;
	if (!construct(lencode, lengths, 19))
		return false;

	index = 0;
	while (index < nlen + ndist) {
		int symbol = decode(lencode);
		if (symbol < 0)
			return false;
		if (symbol < 16) {
			lengths[index++] = (short)symbol;
			continue;
		}

		short len = 0;
		if (symbol == 16) {
			if (index == 0)
				return false; // nothing to repeat
			len = lengths[index - 1];
			symbol = 3 + bits(2);
		} else if (symbol == 17) {
			symbol = 3 + bits(3);
		} else {
			symbol = 11 + bits(7);
		}
		if (_error || index + symbol > nlen + ndist)
			return false;
		while (symbol--)
			lengths[index++] = len;
	}

	if (lengths[256] == 0)
		return false; // no end-of-block code
	if (!construct(lencode, lengths, nlen) || !construct(distcode, lengths + nlen, ndist))
		return false;
	return codes(lencode, distcode);
}

bool Inflater::run() {
	int last;
	do {
		last = bits(1);
		int type = bits(2);
		if (_error)
			return false;
		bool ok;
		switch (type) {
		case 0:
			ok = stored();
			break;
		case 1:
			ok = fixed();
			break;
		case 2:
			ok = dynamic();
			break;
		default:
			ok = false;
			break;
		}
		if (!ok || _error)
			return false;
	} while (!last);
	return true;
}

} // End of anonymous namespace

bool inflateRaw(const byte *src, size_t srcLen, byte *dst, size_t dstLen) {
	Inflater inflater(src, srcLen, dst, dstLen);
	return inflater.run() && inflater.outputSize() == dstLen;
}

uint32 crc32(const byte *data, size_t len, uint32 crc) {
	struct Table {
		uint32 entries[256];
		Table() {
			for (uint32 i = 0; i < 256; i++) {
				uint32 c = i;
				for (int k = 0; k < 8; k++)
					c = (c & 1) ? 0xEDB88320 ^ (c >> 1) : c >> 1;
				entries[i] = c;
			}
		}
	};
	static const Table table;

	crc = ~crc;
	for (size_t i = 0; i < len; i++)
		crc = table.entries[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
	return ~crc;
}

} // End of namespace Common
