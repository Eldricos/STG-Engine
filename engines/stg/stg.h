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
#include "common/endian.h"
#include "common/events.h"
#include "common/random.h"
#include "engines/engine.h"
#include "engines/game.h"
#include "engines/stg/campaign.h"
#include "engines/stg/font.h"
#include "engines/stg/hudscreen.h"
#include "engines/stg/level.h"
#include "engines/stg/mission.h"
#include "engines/stg/optionsmenu.h"
#include "engines/stg/helditem.h"
#include "engines/stg/overlay.h"
#include "engines/stg/render3d.h"
#include "engines/stg/resources.h"
#include "engines/stg/screen.h"
#include "engines/stg/sprite.h"
#include "engines/stg/walker.h"

namespace STG {

const uint32 kSaveMagic = MKTAG('S', 'T', 'G', 'S');
// 1: the viewer's place; 2: adds a mission in progress; 3: doors, lifts,
// lights and the rest the other missions need; 4: the date saved and the
// campaign. Missions saved by an older version can't be read.
const uint32 kSaveVersion = 4;
const uint32 kFirstMissionSaveVersion = 3;
const uint32 kFirstDatedSaveVersion = 4;

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
	/** The 3D page with the away-team console (HudScreen). */
	void drawMissionScreen(uint32 delta);
	/** Blend the screen effects into the level's palette. */
	void applyScreenEffects();

	void showImage(int index);
	void showOverlay(const ResourceRef &ref);
	void showSprites(const ResourceRef &ref);
	/** Load a level; with run false its scripts are set up but not started (a saved game follows). */
	void showLevel(const ResourceRef &ref, bool run = true);
	/** Page 0 is the map, page 1 the 3D view, page n + 1 the n-th texture. */
	void showLevelPage(int page);
	/** Handle a key on the 3D view; returns false if it isn't a movement key. */
	bool handleWalkKey(Common::KeyCode key, bool down);
	/** Move the camera by the keys held, over delta ms, keeping it on the floor. */
	void updateWalk(uint32 delta);
	void updateWalkStatus();
	/** Set up the level's actors, loading their sprite sets. */
	void loadActors(const ResourceRef &levelRef);
	/**
	 * Find a sound file (or the subtitle next to it) by file name,
	 * preferring archives whose name starts with archivePrefix.
	 */
	bool findSoundFile(const Common::String &name, const Common::String &archivePrefix, ResourceRef &ref) const;
	/** Load the level's GAME.BIN, if there is one, and start its mission. */
	void startMission(const ResourceRef &levelRef, bool run = true);
	/** Jump to the first image of the next (direction 1) or previous (-1) archive. */
	void jumpArchive(int direction);
	/** Show the next (1) or previous (-1) 3D level, GAME.LV2. */
	void jumpLevel(int direction);
	void showStarfield();
	/** The starfield with nothing loaded over it, where the mission list is. */
	bool isOnStarfield() const;
	void drawMissionList();
	/** Start an away mission from the list, in the 3D view. */
	void playMission(int index);
	/** Back to the list, saying why. */
	void leaveMission(const Common::String &message);
	/** Before rendering a w x h view: where to aim, the item held up, what to pick. */
	void prepareAim(int w, int h);
	/** After rendering it: the gun held up, or the punch and a crosshair at the centre. */
	void drawHands(Graphics::Surface &view, uint32 delta);
	/** The scripts' full-view overlay (SHOW_VIEW_OVERLAY), if any. */
	void drawViewOverlay(Graphics::Surface &view);
	/** The mouse cursor: a crosshair to aim with, or an arrow. */
	void setCursor(bool target);
	/** The player attacks: fires the weapon in use, or else punches. */
	void attack();
	/** Run the punch animation by delta ms and draw it over view. */
	void updatePunch(uint32 delta, Graphics::Surface &view);
	void playSound(int index);
	void playBeep(int frequency);
	/**
	 * Play one of the movies (e.g. "MOVIE/MOV_01.AVI") full screen, with
	 * its sound, until it ends or a key or click skips it. False if it
	 * can't be found or read.
	 */
	bool playMovie(const Common::String &name);

	// The front end (frontend.cpp).
	/** A line of rolling text: its gap above, and whether to centre it. */
	struct RollLine {
		Common::String text;
		int gap;
		bool centred;
	};
	/**
	 * The title, then the menu, and the story from STATES.INI; false (and
	 * the resource viewer) if the data for it isn't there.
	 */
	bool startFrontEnd();
	void loadSettings();
	/** Keep the settings and apply the volumes. */
	void saveSettings();
	/** Run the story until a mission starts or the game ends. */
	void runCampaign();
	bool startCampaignMission(int type);
	/** The campaign's mission is over: on to whatever comes next. */
	void campaignMissionEnded();
	void openMenu(bool canReturn);
	void closeMenu();
	void handleMenuEvent(const Common::Event &event);
	void playMenuSound(int id);
	std::vector<OptionsMenu::Slot> listSaves();
	bool readSaveHeader(int slot, Common::String &description, Common::String &date);
	static Common::String saveDate();
	/** Poll events; true on a key or click (or quitting). */
	bool waitForSkip();
	/** Show a bitmap full screen in its own palette. */
	bool showPicture(const ResourceRef &ref, Image::BitmapDecoder &decoder);
	void showIntro();
	/** Play a track from MUSIC.1 or MUSIC.2. */
	bool playMusic(const Common::String &name);
	/** Roll lines of text up over a background while the music plays. */
	void rollText(const Image::BitmapDecoder &background, const Font &font, const std::vector<RollLine> &lines,
	              const std::vector<Common::String> &music);
	/** The ending texts, TEXTn.TXT. */
	void showScrollText(int number);
	bool loadCredits(std::vector<RollLine> &lines);
	void showCredits();

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
	/** The mission's BRIEFING.BMP, for its opening transition. */
	Image::BitmapDecoder _briefing;
	bool _briefingLoaded = false;
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
	ResourceRef _levelRef;
	std::vector<uint16> _levelTextures;
	int _levelPage = 0;
	std::unique_ptr<LevelRenderer> _levelRenderer;
	std::unique_ptr<Walker> _walker;
	std::unique_ptr<Actors> _actors;
	std::vector<SpriteDraw> _actorSprites;
	std::unique_ptr<ScriptFile> _levelScript;
	/** Declared last of the level's parts, so it goes first. */
	std::unique_ptr<Mission> _mission;
	/** The away-team console, when the game's art for it is there. */
	std::unique_ptr<HudScreen> _hudScreen;
	/** Whether screen effects changed the palette last frame. */
	bool _paletteBlended = false;
	Camera _camera;
	uint32 _levelTime = 0;
	enum WalkKey { kWalkForward, kWalkBack, kWalkTurnLeft, kWalkTurnRight, kWalkStrafeLeft, kWalkStrafeRight,
	               kWalkLookUp, kWalkLookDown, kWalkKeyCount };
	bool _walkKeys[kWalkKeyCount] = {};
	// The mouse, where the weapon aims (in the view), and the view's size.
	int _mouseX = -1, _mouseY = -1;
	int _aimX = 0, _aimY = 0, _viewW = 1, _viewH = 1;
	bool _targetCursor = false;
	HeldItem _heldItem;
	/** How much longer the gun shows its muzzle flash, in ms. */
	uint32 _flashLeft = 0;
	/** Set while a fire key is held: one attack per press. */
	bool _fireHeld = false;
	Overlay _viewOverlay;
	Common::String _viewOverlayImage;
	bool _viewOverlayLoaded = false;
	// The bare-hand attack's overlay, as the original's view.vt130 runs it.
	Overlay _punchOverlay;
	Common::String _punchImage;
	int _punchFrameHeight = 0;
	bool _punching = false;
	bool _punchLanded = false;
	int _punchFrame = 0;
	uint32 _punchTime = 0;
	int16 _shownHealth = -1;
	int _imageIndex = -1;
	int _soundIndex = -1;
	Audio::SoundHandle _soundHandle;
	Common::String _status;   // first line of the text panel
	Common::String _subtitle; // shown while the current sound plays
	/** The subtitle is a mission's, shown until _subtitleUntil (level time). */
	bool _missionSubtitle = false;
	uint32 _subtitleUntil = 0;
	// The away missions installed, and the one chosen or being played.
	struct MissionEntry {
		Common::String name;
		ResourceRef level;
	};
	std::vector<MissionEntry> _missions;
	int _movieChoice = 0;
	int _missionChoice = 0;
	bool _playingMission = false;
	Common::String _missionMessage;
	/** GET_ARRAY/SET_ARRAY's words, carried from one mission to the next. */
	std::vector<int16> _carriedArray;
	bool _dirty = true;

	/** Playing the game, rather than viewing its files. */
	bool _frontEnd = false;
	Campaign _campaign;
	std::unique_ptr<OptionsMenu> _menu;
	bool _menuOpen = false;
	/** The palette under the menu. */
	Graphics::Palette _gamePalette;
	GameSettings _settings;
	Audio::SoundHandle _menuSound;
	Audio::SoundHandle _musicHandle;
};

} // End of namespace STG

#endif
