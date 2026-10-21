/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_AUDIO_RAW_H
#define COMMON_AUDIO_RAW_H

#include <memory>
#include <vector>

#include "common/audio/audiostream.h"
#include "common/types.h"

namespace Audio {

enum RawFlags {
	/** Samples are unsigned (the usual case for 8-bit PCM). */
	FLAG_UNSIGNED = 1 << 0,
	FLAG_16BITS = 1 << 1,
	FLAG_LITTLE_ENDIAN = 1 << 2,
	FLAG_STEREO = 1 << 3
};

/** Create a stream that plays PCM data held in memory. */
std::unique_ptr<RewindableAudioStream> makeRawStream(std::vector<byte> &&data, int rate, byte flags);

} // End of namespace Audio

#endif
