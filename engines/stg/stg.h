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
#include "engines/stg/font.h"
#include "engines/stg/level.h"
#include "engines/stg/overlay.h"
#include "engines/stg/resources.h"
#include "engines/stg/screen.h"
#include "engines/stg/sprite.h"

namespace STG {

/**
 * Star Trek: Generations engine.
 *
 * The game logic is still to be reverse engineered, so for now the engine
 * runs a viewer over the game's own data: every image, sound and music
 * track in its archives, with speech subtitles drawn in the game's font.
 * This exercises every layer (detection, archives, graphics, palette,
 * cursor, input, audio, saves) and gives the reverse engineering work
 * somewhere to land.
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
	void loadFont();
	void handleEvent(const Common::Event &event);
	void drawFrame(uint32 delta);
	void drawStarfield(uint32 delta);
	void drawTextPanel();

	void showImage(int index);
	void showOverlay(const ResourceRef &ref);
	void showSprites(const ResourceRef &ref);
	void showLevel(const ResourceRef &ref);
	/** Page 0 is the map, page n the level's n-th texture. */
	void showLevelPage(int page);
	/** Jump to the first image of the next (direction 1) or previous (-1) archive. */
	void jumpArchive(int direction);
	void showStarfield();
	void playSound(int index);
	void playBeep(int frequency);

	Common::String getSaveFileName(int slot) const;

	DetectedGame _game;
	Common::RandomSource _random;
	std::unique_ptr<Screen> _screen;
	ResourceManager _resources;

	std::vector<Star> _stars;
	Font _font;
	Graphics::Palette _fontPalette;
	bool _hasFontPalette = false;
	Image::BitmapDecoder _image;
	bool _imageLoaded = false;
	Overlay _overlay;
	bool _overlayLoaded = false;
	int _overlayFrameHeight = 0;
	int _overlayFrame = 0;
	uint32 _overlayTime = 0; // also used to animate sprites
	SpriteSet _sprites;
	bool _spritesLoaded = false;
	std::unique_ptr<LevelFile> _level;
	Common::String _levelName;
	std::vector<uint16> _levelTextures;
	int _levelPage = 0;
	int _imageIndex = -1;
	int _soundIndex = -1;
	Audio::SoundHandle _soundHandle;
	Common::String _status;   // first line of the text panel
	Common::String _subtitle; // shown while the current sound plays
	bool _dirty = true;
};

} // End of namespace STG

#endif
