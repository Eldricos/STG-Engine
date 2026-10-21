/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_RANDOM_H
#define COMMON_RANDOM_H

#include "common/types.h"

namespace Common {

/**
 * A small, deterministic pseudo random number generator. Engines own their
 * own instance so that a recorded seed reproduces a play session exactly.
 */
class RandomSource {
public:
	explicit RandomSource(uint32 seed = 0);

	void setSeed(uint32 seed);
	uint32 getSeed() const { return _seed; }

	/** A random number in the range [0, max]. */
	uint32 getRandomNumber(uint32 max);

	/** A random number in the range [min, max]. */
	uint32 getRandomNumberRng(uint32 min, uint32 max);

	bool getRandomBit();

private:
	uint32 _seed;
	uint32 _state;
};

} // End of namespace Common

#endif
