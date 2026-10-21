/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/engine.h"

#include "common/audio/mixer.h"
#include "common/config-manager.h"
#include "common/events.h"
#include "common/system.h"

Engine *g_engine = nullptr;

Engine::Engine(OSystem *syst, const Common::FSNode &gameDataDir)
	: _system(syst),
	  _mixer(syst->getMixer()),
	  _eventMan(syst->getEventManager()),
	  _saveFileMan(syst->getSavefileManager()),
	  _gameDataDir(gameDataDir) {
	g_engine = this;
}

Engine::~Engine() {
	if (_mixer)
		_mixer->stopAll();
	if (g_engine == this)
		g_engine = nullptr;
}

bool Engine::shouldQuit() const {
	return _eventMan->shouldQuit() || _eventMan->shouldReturnToLauncher();
}

void Engine::quitGame() {
	Common::Event event;
	event.type = Common::EVENT_QUIT;
	_eventMan->pushEvent(event);
}

void Engine::pauseEngine(bool pause) {
	if (pause)
		_pauseLevel++;
	else if (_pauseLevel > 0)
		_pauseLevel--;

	if ((pause && _pauseLevel == 1) || (!pause && _pauseLevel == 0))
		pauseEngineIntern(pause);
}

void Engine::pauseEngineIntern(bool pause) {
	if (_mixer)
		_mixer->pauseAll(pause);
}

void Engine::syncSoundSettings() {
	if (!_mixer)
		return;

	bool mute = ConfMan.getBool("mute");
	int music = mute ? 0 : ConfMan.getInt("music_volume");
	int sfx = mute ? 0 : ConfMan.getInt("sfx_volume");
	int speech = mute ? 0 : ConfMan.getInt("speech_volume");

	_mixer->setVolumeForSoundType(Audio::Mixer::kMusicSoundType, music);
	_mixer->setVolumeForSoundType(Audio::Mixer::kSFXSoundType, sfx);
	_mixer->setVolumeForSoundType(Audio::Mixer::kSpeechSoundType, speech);
	_mixer->setVolumeForSoundType(Audio::Mixer::kPlainSoundType, Audio::Mixer::kMaxMixerVolume);
}

Common::Error Engine::loadGameState(int slot) {
	return Common::Error(Common::kUnknownError, "This engine does not support loading");
}

Common::Error Engine::saveGameState(int slot, const Common::String &description) {
	return Common::Error(Common::kUnknownError, "This engine does not support saving");
}
