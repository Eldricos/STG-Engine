/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_AUDIO_AUDIOSTREAM_H
#define COMMON_AUDIO_AUDIOSTREAM_H

#include <memory>

#include "common/types.h"

namespace Audio {

/**
 * A source of signed 16-bit native-endian PCM. Stereo streams interleave
 * left and right samples.
 */
class AudioStream {
public:
	virtual ~AudioStream() {}

	/**
	 * Fill buffer with up to numSamples samples (not frames: for stereo,
	 * numSamples counts left and right separately). Returns the number of
	 * samples written.
	 */
	virtual int readBuffer(int16 *buffer, int numSamples) = 0;

	virtual bool isStereo() const = 0;
	virtual int getRate() const = 0;

	/** No data is available right now (it may arrive later for queued streams). */
	virtual bool endOfData() const = 0;

	/** No data will ever be available again. */
	virtual bool endOfStream() const { return endOfData(); }
};

/** A stream that can restart from the beginning. */
class RewindableAudioStream : public AudioStream {
public:
	virtual bool rewind() = 0;
};

/**
 * Play a rewindable stream a number of times; loops == 0 means forever.
 */
std::unique_ptr<AudioStream> makeLoopingAudioStream(std::unique_ptr<RewindableAudioStream> stream, uint loops);

} // End of namespace Audio

#endif
