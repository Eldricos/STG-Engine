/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_AUDIO_WAVE_H
#define COMMON_AUDIO_WAVE_H

#include <memory>

#include "common/audio/audiostream.h"
#include "common/stream.h"

namespace Audio {

struct WAVInfo {
	uint16 formatTag = 0;
	uint16 channels = 0;
	uint32 sampleRate = 0;
	uint16 bitsPerSample = 0;
	uint16 blockAlign = 0;
	/** Offset and size of the "data" chunk. */
	uint32 dataOffset = 0;
	uint32 dataSize = 0;
};

/**
 * Parse a RIFF WAVE header, leaving the stream positioned at the start
 * of the sample data. Returns false if the stream is not a WAVE file.
 */
bool loadWAVFromStream(Common::SeekableReadStream &stream, WAVInfo &info);

/**
 * Create an audio stream from a WAVE file. Uncompressed PCM is supported;
 * other encodings are reported and rejected.
 */
std::unique_ptr<RewindableAudioStream> makeWAVStream(Common::SeekableReadStream &stream);

} // End of namespace Audio

#endif
