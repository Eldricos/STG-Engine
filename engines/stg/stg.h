/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_STG_H
#define STG_STG_H

#include <memory>
#include <vector>

#include "common/audio/mixer.h"
#include "common/events.h"
#include "common/random.h"
#include "engines/engine.h"
#include "engines/game.h"
#include "engines/stg/resources.h"
#include "engines/stg/screen.h"

namespace STG {

/**
 * Star Trek: Generations engine.
 *
 * The game's own formats are still to be reverse engineered, so for now
 * the engine runs a resource viewer over the standard Windows files the
 * game ships with, alongside a test pattern. This exercises every layer
 * (detection, files, graphics, palette, cursor, input, audio, saves) and
 * gives the reverse engineering work somewhere to land.
 */
class STGEngine : public Engine {
public:
	STGEngine(OSystem *syst, const DetectedGame &game);
	~STGEngine() override;

	Common::Error run() override;

	bool canLoadGameStateCurrently() override { return true; }
	bool canSaveGameStateCurrently() override { return true; }
	Common::Error loadGameState(int slot) override;
	Common::Error saveGameState(int slot, const Common::String &description) override;

	const DetectedGame &getGame() const { return _game; }
	Common::RandomSource &getRandom() { return _random; }

private:
	struct Star {
		int32 x, y, z;
	};

	void initPalette();
	void initCursor();
	void handleEvent(const Common::Event &event);
	void drawFrame(uint32 delta);
	void drawStarfield(uint32 delta);

	void showImage(int index);
	void playSound(int index);
	void playBeep(int frequency);

	Common::String getSaveFileName(int slot) const;

	DetectedGame _game;
	Common::RandomSource _random;
	std::unique_ptr<Screen> _screen;
	ResourceManager _resources;

	std::vector<Star> _stars;
	Image::BitmapDecoder _image;
	bool _imageLoaded = false;
	int _imageIndex = -1;
	int _soundIndex = -1;
	Audio::SoundHandle _soundHandle;
	bool _dirty = true;
};

} // End of namespace STG

#endif
