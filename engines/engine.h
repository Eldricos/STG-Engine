/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef ENGINES_ENGINE_H
#define ENGINES_ENGINE_H

#include "common/error.h"
#include "common/fs.h"
#include "common/str.h"

class OSystem;

namespace Audio {
class Mixer;
}

namespace Common {
class EventManager;
class SaveFileManager;
}

/**
 * Base class for game engines.
 *
 * An engine only talks to the platform through the OSystem pointer and the
 * managers it hands out (events, mixer, save files). It never includes SDL
 * or any other host API; the build enforces this by not giving engine
 * targets SDL's include paths.
 */
class Engine {
public:
	Engine(OSystem *syst, const Common::FSNode &gameDataDir);
	virtual ~Engine();

	Engine(const Engine &) = delete;
	Engine &operator=(const Engine &) = delete;

	/** Run the game until the user quits. */
	virtual Common::Error run() = 0;

	/** True once the user has asked to quit or return to the launcher. */
	bool shouldQuit() const;

	/** Ask the engine to exit at the next opportunity. */
	void quitGame();

	void pauseEngine(bool pause);
	bool isPaused() const { return _pauseLevel > 0; }

	/** Apply volume settings from the configuration to the mixer. */
	virtual void syncSoundSettings();

	virtual bool canLoadGameStateCurrently() { return false; }
	virtual bool canSaveGameStateCurrently() { return false; }
	virtual Common::Error loadGameState(int slot);
	virtual Common::Error saveGameState(int slot, const Common::String &description);

	const Common::FSNode &getGameDataDir() const { return _gameDataDir; }

protected:
	/** Called when the pause state changes; the default pauses all sound. */
	virtual void pauseEngineIntern(bool pause);

	OSystem *_system;
	Audio::Mixer *_mixer;
	Common::EventManager *_eventMan;
	Common::SaveFileManager *_saveFileMan;
	const Common::FSNode _gameDataDir;

private:
	int _pauseLevel = 0;
};

/** The currently running engine, or nullptr. */
extern Engine *g_engine;

#endif
