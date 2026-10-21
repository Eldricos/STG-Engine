/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "common/audio/wave.h"
#include "common/audio/raw.h"
#include "common/debug.h"

namespace Audio {

static const uint16 kWaveFormatPCM = 1;

bool loadWAVFromStream(Common::SeekableReadStream &stream, WAVInfo &info) {
	int64 start = stream.pos();
	if (stream.readUint32BE() != MKTAG('R', 'I', 'F', 'F'))
		return false;
	uint32 riffSize = stream.readUint32LE();
	if (stream.readUint32BE() != MKTAG('W', 'A', 'V', 'E'))
		return false;

	int64 riffEnd = start + 8 + riffSize;
	bool haveFormat = false;

	while (stream.pos() + 8 <= riffEnd && !stream.eos()) {
		uint32 tag = stream.readUint32BE();
		uint32 size = stream.readUint32LE();
		int64 chunkStart = stream.pos();

		if (tag == MKTAG('f', 'm', 't', ' ')) {
			info.formatTag = stream.readUint16LE();
			info.channels = stream.readUint16LE();
			info.sampleRate = stream.readUint32LE();
			stream.readUint32LE(); // average bytes per second
			info.blockAlign = stream.readUint16LE();
			info.bitsPerSample = stream.readUint16LE();
			haveFormat = true;
		} else if (tag == MKTAG('d', 'a', 't', 'a')) {
			if (!haveFormat) {
				warning("WAVE file has data before fmt chunk");
				return false;
			}
			info.dataOffset = (uint32)chunkStart;
			// Some writers of the period got the data size wrong; trust the file length.
			int64 available = stream.size() - chunkStart;
			info.dataSize = (int64)size > available ? (uint32)available : size;
			return true;
		}

		// Chunks are padded to an even length.
		stream.seek(chunkStart + size + (size & 1));
	}
	return false;
}

std::unique_ptr<RewindableAudioStream> makeWAVStream(Common::SeekableReadStream &stream) {
	WAVInfo info;
	if (!loadWAVFromStream(stream, info)) {
		warning("makeWAVStream: not a valid WAVE file");
		return nullptr;
	}

	if (info.formatTag != kWaveFormatPCM) {
		warning("makeWAVStream: unsupported WAVE encoding 0x%04x", info.formatTag);
		return nullptr;
	}
	if ((info.bitsPerSample != 8 && info.bitsPerSample != 16) || info.channels < 1 || info.channels > 2) {
		warning("makeWAVStream: unsupported layout %d channels, %d bits", info.channels, info.bitsPerSample);
		return nullptr;
	}

	byte flags = 0;
	if (info.bitsPerSample == 8)
		flags |= FLAG_UNSIGNED;
	else
		flags |= FLAG_16BITS | FLAG_LITTLE_ENDIAN;
	if (info.channels == 2)
		flags |= FLAG_STEREO;

	std::vector<byte> data(info.dataSize);
	stream.seek(info.dataOffset);
	data.resize(stream.read(data.data(), info.dataSize));
	return makeRawStream(std::move(data), (int)info.sampleRate, flags);
}

} // End of namespace Audio
