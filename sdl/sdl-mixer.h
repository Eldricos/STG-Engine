/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SDL_SDL_MIXER_H
#define SDL_SDL_MIXER_H

#include <SDL3/SDL.h>

#include <memory>
#include <vector>

#include "common/audio/mixer.h"

/**
 * Opens the SDL audio device and feeds it from the common software mixer.
 * If no device is available the mixer still exists but stays silent, so
 * engines never have to special-case missing audio.
 */
class SdlMixerManager {
public:
	static const uint kOutputRate = 44100;

	SdlMixerManager();
	~SdlMixerManager();

	void init();
	Audio::Mixer *getMixer() { return _mixer.get(); }

private:
	static void SDLCALL audioCallback(void *userdata, SDL_AudioStream *stream, int additionalAmount, int totalAmount);

	std::unique_ptr<Audio::Mixer> _mixer;
	SDL_AudioStream *_stream = nullptr;
	std::vector<int16> _buffer;
};

#endif
