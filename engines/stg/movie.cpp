/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/movie.h"

#include <algorithm>
#include <cstring>

namespace STG {

namespace {

uint32 readBE16(const byte *p) { return (uint32)p[0] << 8 | p[1]; }
uint32 readBE24(const byte *p) { return (uint32)p[0] << 16 | (uint32)p[1] << 8 | p[2]; }
uint32 readBE32(const byte *p) { return (uint32)p[0] << 24 | readBE24(p + 1); }

uint32 tag(const char *s) {
	return (uint32)(byte)s[0] | (uint32)(byte)s[1] << 8 | (uint32)(byte)s[2] << 16 | (uint32)(byte)s[3] << 24;
}

int clip(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }

} // End of anonymous namespace

CinepakDecoder::CinepakDecoder(int width, int height)
	: _width(width), _height(height), _picture((size_t)width * height, 0xFF000000) {
	_strips.resize(1);
	std::memset(_strips.data(), 0, sizeof(Strip));
}

uint32 CinepakDecoder::colour(const Cell &cell, int i) const {
	int y = cell.y[i];
	return 0xFF000000u | (uint32)clip(y + 2 * cell.v) << 16 | (uint32)clip(y - cell.u / 2 - cell.v) << 8 |
	       (uint32)clip(y + 2 * cell.u);
}

void CinepakDecoder::decodeCodebook(Cell *codebook, int chunk, const byte *data, const byte *end) {
	// Greyscale codebooks (0x04) have no colour bytes; partial ones (0x01)
	// say which entries change with a bit each, in 32-bit words.
	int n = (chunk & 0x04) ? 4 : 6;
	uint32 flags = 0, mask = 0;
	for (int i = 0; i < 256; i++) {
		if (chunk & 0x01) {
			mask >>= 1;
			if (!mask) {
				if (data + 4 > end)
					return;
				flags = readBE32(data);
				data += 4;
				mask = 0x80000000;
			}
			if (!(flags & mask))
				continue;
		}
		if (data + n > end)
			return;
		Cell &cell = codebook[i];
		for (int j = 0; j < 4; j++)
			cell.y[j] = *data++;
		if (n == 6) {
			cell.u = (int8)*data++;
			cell.v = (int8)*data++;
		} else {
			cell.u = cell.v = 0;
		}
	}
}

bool CinepakDecoder::decodeVectors(const Strip &strip, int chunk, int top, int bottom, const byte *data, const byte *end) {
	// 0x30: every block coded, a bit each for V1 or V4. 0x31: a bit for
	// skipped or coded first. 0x32: every block V1.
	uint32 flags = 0, mask = 0;
	auto nextBit = [&](bool &bit) {
		mask >>= 1;
		if (!mask) {
			if (data + 4 > end)
				return false;
			flags = readBE32(data);
			data += 4;
			mask = 0x80000000;
		}
		bit = (flags & mask) != 0;
		return true;
	};
	auto put = [&](int x, int y, uint32 c) {
		if (x < _width && y < bottom)
			_picture[(size_t)y * _width + x] = c;
	};

	for (int y = top; y < bottom; y += 4) {
		for (int x = 0; x < _width; x += 4) {
			bool bit;
			if (chunk & 0x01) {
				if (!nextBit(bit))
					return false;
				if (!bit)
					continue;
			}
			bool v4 = false;
			if (!(chunk & 0x02)) {
				if (!nextBit(bit))
					return false;
				v4 = bit;
			}
			if (!v4) {
				if (data + 1 > end)
					return false;
				const Cell &cell = strip.v1[*data++];
				for (int i = 0; i < 4; i++) {
					uint32 c = colour(cell, i);
					int cx = x + (i & 1) * 2, cy = y + (i >> 1) * 2;
					put(cx, cy, c);
					put(cx + 1, cy, c);
					put(cx, cy + 1, c);
					put(cx + 1, cy + 1, c);
				}
			} else {
				if (data + 4 > end)
					return false;
				for (int q = 0; q < 4; q++) {
					const Cell &cell = strip.v4[*data++];
					int cx = x + (q & 1) * 2, cy = y + (q >> 1) * 2;
					for (int i = 0; i < 4; i++)
						put(cx + (i & 1), cy + (i >> 1), colour(cell, i));
				}
			}
		}
	}
	return true;
}

bool CinepakDecoder::decode(const byte *data, uint32 size) {
	if (size == 0)
		return true; // A dropped frame: the last one stays up.
	if (size < 10)
		return false;
	const byte *end = data + size;
	byte frameFlags = data[0];
	int strips = (int)readBE16(data + 8);
	data += 10;

	if ((int)_strips.size() < strips) {
		size_t old = _strips.size();
		_strips.resize(strips);
		std::memset(&_strips[old], 0, (strips - old) * sizeof(Strip));
	}

	int y0 = 0;
	for (int i = 0; i < strips; i++) {
		if (data + 12 > end)
			return false;
		uint32 stripSize = readBE24(data + 1);
		int y1 = (int)readBE16(data + 4);
		int y2 = (int)readBE16(data + 8);
		// A strip whose top is zero is placed under the last one.
		if (y1 == 0) {
			y1 = y0;
			y2 += y0;
		}
		if (stripSize < 12 || data + stripSize > end)
			stripSize = (uint32)(end - data);
		const byte *stripEnd = data + stripSize;
		data += 12;

		Strip &strip = _strips[i];
		if (i > 0 && !(frameFlags & 0x01))
			strip = _strips[i - 1];

		y2 = std::min(y2, _height);
		while (data + 4 <= stripEnd) {
			int chunk = data[0];
			uint32 chunkSize = readBE24(data + 1);
			if (chunkSize < 4 || data + chunkSize > stripEnd)
				chunkSize = (uint32)(stripEnd - data);
			const byte *body = data + 4, *bodyEnd = data + chunkSize;
			switch (chunk) {
			case 0x20: case 0x21: case 0x24: case 0x25:
				decodeCodebook(strip.v4, chunk, body, bodyEnd);
				break;
			case 0x22: case 0x23: case 0x26: case 0x27:
				decodeCodebook(strip.v1, chunk, body, bodyEnd);
				break;
			case 0x30: case 0x31: case 0x32:
				decodeVectors(strip, chunk, y1, y2, body, bodyEnd);
				break;
			default:
				break;
			}
			data += chunkSize;
		}
		data = stripEnd;
		y0 = y2;
	}
	return true;
}

bool Movie::load(std::unique_ptr<Common::SeekableReadStream> stream) {
	_stream = std::move(stream);
	_frames.clear();
	_audio.clear();
	_current = -1;
	if (!_stream || _stream->readUint32LE() != tag("RIFF"))
		return false;
	uint32 size = _stream->readUint32LE();
	if (_stream->readUint32LE() != tag("AVI "))
		return false;
	uint32 end = (uint32)std::min<int64>((int64)size + 8, _stream->size());
	if (!readList(end, 0) || _videoStream < 0 || _width <= 0 || _height <= 0)
		return false;
	if (_videoHandler != tag("cvid") && _videoHandler != tag("CVID"))
		return false;
	_decoder.reset(new CinepakDecoder(_width, _height));
	return true;
}

bool Movie::readList(uint32 end, int depth) {
	if (depth > 8)
		return false;
	int streamType = -1; // 0 video, 1 audio, within a strl list
	while ((uint32)_stream->pos() + 8 <= end) {
		uint32 id = _stream->readUint32LE();
		uint32 size = _stream->readUint32LE();
		uint32 start = (uint32)_stream->pos();
		uint32 next = start + size + (size & 1);
		if (_stream->err() || start + size > end)
			break;

		if (id == tag("LIST")) {
			uint32 type = _stream->readUint32LE();
			if (type == tag("strl"))
				_streams++;
			if (!readList(start + size, depth + 1))
				return false;
			(void)type;
		} else if (id == tag("avih")) {
			_frameTime = _stream->readUint32LE();
			if (!_frameTime)
				_frameTime = 41666;
		} else if (id == tag("strh")) {
			uint32 type = _stream->readUint32LE();
			uint32 handler = _stream->readUint32LE();
			if (type == tag("vids") && _videoStream < 0) {
				_videoStream = _streams - 1;
				_videoHandler = handler;
				streamType = 0;
			} else if (type == tag("auds") && _audioStream < 0) {
				_audioStream = _streams - 1;
				streamType = 1;
			} else {
				streamType = -1;
			}
		} else if (id == tag("strf")) {
			if (streamType == 0 && size >= 12) {
				_stream->readUint32LE(); // biSize
				_width = _stream->readSint32LE();
				_height = std::abs(_stream->readSint32LE());
			} else if (streamType == 1 && size >= 16) {
				uint16 format = _stream->readUint16LE();
				_audioChannels = _stream->readUint16LE();
				_audioRate = (int)_stream->readUint32LE();
				_stream->readUint32LE(); // bytes a second
				_stream->readUint16LE(); // block align
				_audioBits = _stream->readUint16LE();
				if (format != 1) // only PCM
					_audioStream = -2;
			}
		} else if ((id >> 16) == ('d' | 'c' << 8) || (id >> 16) == ('d' | 'b' << 8) ||
		           (id >> 16) == ('i' | 'd' << 8) || (id >> 16) == ('w' | 'b' << 8)) {
			// "nndc"/"nndb" video ("nnid" for the game's inter frames), "nnwb" sound.
			int number = ((id & 0xFF) - '0') * 10 + (((id >> 8) & 0xFF) - '0');
			if ((id >> 16) == ('w' | 'b' << 8)) {
				if (number == _audioStream && size) {
					size_t old = _audio.size();
					_audio.resize(old + size);
					_stream->read(&_audio[old], size);
				}
			} else if (number == _videoStream) {
				_frames.push_back({start, size});
			}
		}
		_stream->seek(next);
	}
	return true;
}

bool Movie::nextFrame() {
	if (_current + 1 >= (int)_frames.size())
		return false;
	_current++;
	const Chunk &chunk = _frames[_current];
	if (chunk.size) {
		std::vector<byte> data(chunk.size);
		_stream->seek(chunk.offset);
		if (_stream->read(data.data(), chunk.size) != chunk.size)
			return false;
		_decoder->decode(data.data(), chunk.size);
	}
	return true;
}

} // End of namespace STG
