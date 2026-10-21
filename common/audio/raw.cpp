/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "common/audio/raw.h"
#include "common/endian.h"

namespace Audio {

namespace {

class RawStream : public RewindableAudioStream {
public:
	RawStream(std::vector<byte> &&data, int rate, byte flags)
		: _data(std::move(data)), _rate(rate), _flags(flags) {
		_sampleSize = (flags & FLAG_16BITS) ? 2 : 1;
		// Drop a trailing partial sample or frame.
		uint frameSize = _sampleSize * (isStereo() ? 2 : 1);
		_data.resize(_data.size() - _data.size() % frameSize);
	}

	int readBuffer(int16 *buffer, int numSamples) override {
		int count = 0;
		while (count < numSamples && _pos < _data.size()) {
			const byte *p = &_data[_pos];
			int16 sample;
			if (_sampleSize == 2) {
				uint16 v = (_flags & FLAG_LITTLE_ENDIAN) ? READ_LE_UINT16(p) : READ_BE_UINT16(p);
				if (_flags & FLAG_UNSIGNED)
					v ^= 0x8000;
				sample = (int16)v;
			} else {
				byte v = *p;
				if (_flags & FLAG_UNSIGNED)
					v ^= 0x80;
				sample = (int16)((int8)v * 256);
			}
			buffer[count++] = sample;
			_pos += _sampleSize;
		}
		return count;
	}

	bool isStereo() const override { return (_flags & FLAG_STEREO) != 0; }
	int getRate() const override { return _rate; }
	bool endOfData() const override { return _pos >= _data.size(); }

	bool rewind() override {
		_pos = 0;
		return true;
	}

private:
	std::vector<byte> _data;
	size_t _pos = 0;
	int _rate;
	byte _flags;
	uint _sampleSize;
};

} // End of anonymous namespace

std::unique_ptr<RewindableAudioStream> makeRawStream(std::vector<byte> &&data, int rate, byte flags) {
	return std::unique_ptr<RewindableAudioStream>(new RawStream(std::move(data), rate, flags));
}

} // End of namespace Audio
