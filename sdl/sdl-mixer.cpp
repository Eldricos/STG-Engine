/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "sdl/sdl-mixer.h"

#include "common/debug.h"

SdlMixerManager::SdlMixerManager() : _mixer(new Audio::Mixer(kOutputRate)) {
}

SdlMixerManager::~SdlMixerManager() {
	// Stop the callback before the mixer it uses goes away.
	if (_stream)
		SDL_DestroyAudioStream(_stream);
	_mixer->setReady(false);
}

void SdlMixerManager::init() {
	if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) {
		warning("Could not initialise audio: %s", SDL_GetError());
		return;
	}

	SDL_AudioSpec spec;
	spec.format = SDL_AUDIO_S16;
	spec.channels = 2;
	spec.freq = (int)kOutputRate;

	_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audioCallback, this);
	if (!_stream) {
		warning("Could not open audio device: %s", SDL_GetError());
		return;
	}

	_mixer->setReady(true);
	SDL_ResumeAudioStreamDevice(_stream);
	debug(1, "Audio output %d Hz, 16-bit stereo", spec.freq);
}

void SDLCALL SdlMixerManager::audioCallback(void *userdata, SDL_AudioStream *stream, int additionalAmount, int totalAmount) {
	SdlMixerManager *manager = (SdlMixerManager *)userdata;
	const int frameSize = 2 * sizeof(int16);
	int frames = additionalAmount / frameSize;
	if (frames <= 0)
		return;

	manager->_buffer.resize(frames * 2);
	manager->_mixer->mixCallback(manager->_buffer.data(), frames);
	SDL_PutAudioStreamData(stream, manager->_buffer.data(), frames * frameSize);
}
