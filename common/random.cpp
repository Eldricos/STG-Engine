/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "common/random.h"

#include <ctime>

namespace Common {

RandomSource::RandomSource(uint32 seed) {
	setSeed(seed ? seed : (uint32)time(nullptr));
}

void RandomSource::setSeed(uint32 seed) {
	_seed = seed;
	_state = seed ? seed : 0x2545F491;
}

uint32 RandomSource::getRandomNumber(uint32 max) {
	// xorshift32
	_state ^= _state << 13;
	_state ^= _state >> 17;
	_state ^= _state << 5;
	if (max == 0xFFFFFFFF)
		return _state;
	return _state % (max + 1);
}

uint32 RandomSource::getRandomNumberRng(uint32 min, uint32 max) {
	return getRandomNumber(max - min) + min;
}

bool RandomSource::getRandomBit() {
	return (getRandomNumber(0xFFFFFFFF) >> 16) & 1;
}

} // End of namespace Common
