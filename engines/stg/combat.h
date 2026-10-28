/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_COMBAT_H
#define STG_COMBAT_H

#include <deque>
#include <map>
#include <memory>
#include <vector>

#include "common/audio/mixer.h"
#include "common/graphics/surface.h"
#include "common/keyboard.h"
#include "common/random.h"
#include "engines/stg/combatdata.h"
#include "engines/stg/combatrender.h"
#include "engines/stg/resources.h"
#include "engines/stg/rgbimage.h"

namespace STG {

/**
 * Space combat (sgens.exe module 0x47B375, the "sc" library): the
 * Enterprise against the ships of COMBAT.DAT's SCn.MIS, seen through the
 * bridge's viewscreen above the tactical console.
 *
 * First the briefing (COMBATn.BMP with its PROCEED button), then the
 * fight: the helm (by hand, or Close, Evade, Maintain distance, Full stop
 * and Warp out), phasers in their firing arcs, photon torpedo salvos,
 * shields by facing, damage to systems, waves of enemies, explosions, the
 * crew's reports with subtitles. The enemies fly the original's own AI
 * (0x4A7C91): they head for the Enterprise until within three quarters of
 * phaser range, then hold their turn until they are out again; they fire
 * every seven seconds, swerve off when about to collide, break off when
 * badly hurt, and Soran keeps away while he has company. The TACTICS
 * scripts (*.BIN) in COMBAT.DAT are left over from development: the game
 * never attaches one to a ship.
 *
 * It ends won (every enemy gone, or Soran's ship beaten), lost (the
 * Enterprise warped out) or with the Enterprise destroyed.
 */
class SpaceCombat {
public:
	enum Result { kRunning, kWon, kLost, kDestroyed };
	static const int kWidth = 640, kHeight = 480;

	SpaceCombat(const ResourceManager &resources, Audio::Mixer *mixer, Common::RandomSource &random);
	~SpaceCombat();

	/** Load combat n (1..25): its briefing, ships and console. */
	bool load(int mission);
	void setSubtitles(bool on) { _subtitles = on; }
	/** 0 Captain (more damage taken), 1 Lieutenant, 2 Ensign (less). */
	void setDifficulty(int difficulty) { _difficulty = difficulty; }

	void update(uint32 ms);
	void draw(Graphics::Surface &screen);
	void keyDown(const Common::KeyState &key);
	void keyUp(const Common::KeyState &key);
	void mouseMove(int x, int y);
	void mouseDown(int x, int y);
	void mouseUp(int x, int y);
	/** Esc: the options menu. */
	bool takeOptionsRequest();

	/** Over, once the closing speech and music have had their time. */
	bool isFinished() const { return _finished; }
	Result getResult() const { return _result; }

	/** Stop every sound (when the menu opens, say). */
	void silence();

private:
	struct Ship;
	struct Torpedo {
		Vec3 position, velocity;
		int owner, target;
		uint32 life;
		int damage;
		const SprAnimation *sprite;
	};
	struct Beam {
		int from, to;
		Vec3 end;
		uint32 time;
		uint32 colour;
		int damage, facingFrom;
		bool hit, applied;
	};
	struct Explosion {
		Vec3 position;
		double size;
		uint32 time;
		const SprAnimation *sprite;
	};
	enum Helm { kClose = 0, kEvade = 1, kMaintain = 2, kFullStop = 3, kStandBy = 4, kWarpOut = 5, kMaintainStatus = 6 };
	enum Phase { kBriefing, kFight, kEnding };

	// Data.
	std::unique_ptr<Common::SeekableReadStream> open(const Common::String &name) const;
	const CombatImage *image(const Common::String &name);
	const CombatImage *texture(const Common::String &name);
	const SprAnimation *sprite(const Common::String &name);
	const Model3D *model(const Common::String &name);
	const std::vector<CombatMaterial> *materials(const Common::String &name);
	const ShipClass *findClass(const Common::String &token) const;
	void addShip(const MissionShip &line);

	// Simulation.
	void updateShip(Ship &ship, double dt);
	void steer(Ship &ship, const Vec3 &point, double dt, bool away = false);
	void fly(Ship &ship, double dt);
	void updateAi(Ship &ship, double dt);
	/** Head for a point well off the Enterprise (0x4AFF2A). */
	void runOff(Ship &ship);
	bool isCollisionImminent(const Ship &a, const Ship &b) const;
	void updateHelm(double dt);
	void firePhasers(int shooter);
	void firePhotons(int shooter);
	void updateWeapons(uint32 ms);
	/** Which shield of target faces a point (0x54E278). */
	int facing(const Ship &target, const Vec3 &from) const;
	/** The octant of a point seen from a ship: 4 starboard, 2 aft, 1 dorsal. */
	int octant(const Ship &ship, const Vec3 &point) const;
	void hit(int target, int facing, double damage, int aimed);
	void destroy(int ship);
	void nextTarget(bool announce);
	void checkEnd();
	void finish(Result result);
	double systemFraction(const Ship &ship, int system) const;
	bool isEnemyAlive(const Ship &ship) const;

	// Sound.
	void speak(int id, bool force = false);
	void playEffect(const Common::String &name, int volume = 255);
	void playMusic(const Common::String &name, bool loop);

	// Drawing.
	void drawView(Graphics::Surface &screen);
	void drawConsole(Graphics::Surface &screen);
	void drawImage(Graphics::Surface &screen, const CombatImage *image, int x, int y, const Common::Rect *clip = nullptr);
	void drawText(Graphics::Surface &screen, const Common::String &text, int x, int y, bool small = false);
	void drawBars(Graphics::Surface &screen, const Ship &ship, int x, int y);
	void drawRadar(Graphics::Surface &screen);

	const ResourceManager &_resources;
	Audio::Mixer *_mixer;
	Common::RandomSource &_random;
	int _mission = 0;
	int _difficulty = 1;
	bool _subtitles = true;

	std::vector<ShipClass> _classes;
	std::map<Common::String, std::unique_ptr<CombatImage>> _images;
	std::map<Common::String, std::unique_ptr<SprAnimation>> _sprites;
	std::map<Common::String, std::unique_ptr<Model3D>> _models;
	std::map<Common::String, std::unique_ptr<std::vector<CombatMaterial>>> _materials;
	uint32 _palette[256] = {};
	MetFont _font, _smallFont;
	RgbImage _briefing;
	RgbImage _proceed;
	int _proceedX = 535, _proceedY = 429;

	std::vector<Ship> _ships; // [0] the Enterprise
	std::vector<Torpedo> _torpedoes;
	std::vector<Beam> _beams;
	std::vector<Explosion> _explosions;
	std::vector<Vec3> _stars;
	CombatRenderer _renderer;

	Phase _phase = kBriefing;
	Result _result = kRunning;
	bool _finished = false;
	uint32 _endTime = 0;
	uint32 _time = 0;
	int _target = -1;
	int _aimedSystem = -1;
	Helm _helm = kStandBy;
	/** The CONN line (hotspot 2), which calls up the helm menu over the Enterprise's panel. */
	const Common::Rect kConnLine = Common::Rect(262, 440, 439, 461);
	bool _helmMenu = false;
	/**
	 * The console's sliders (0x495B49): a slot the knob follows the mouse
	 * down, 18 steps from top to bottom, drawn unlit (the N picture) above
	 * the knob and lit (F) below.
	 */
	struct Slider {
		Common::Rect slot;
		const char *unlit, *lit;
		/** The row of the slot the mouse last set. */
		int row;
		/** 0 at the top .. 18 at the bottom. */
		int step() const { return 18 * (row + 1) / slot.height(); }
	};
	/**
	 * The viewscreen zoom (hotspot 14): 18 - step, 0..18 (0x4AEDFE,
	 * 0x54A0B0), 0 at the start, the slot unlit.
	 */
	Slider _viewSlider = { Common::Rect(618, 127, 634, 181), "SVIEWN.RAS", "SVIEWF.RAS", 54 };
	/**
	 * The radar's range (hotspot 13): (19 - step) x 7500 + 10 (0x4AB7E2),
	 * at step 12 to start with (0x49D640), 52510.
	 */
	Slider _radarSlider = { Common::Rect(450, 317, 466, 372), "SRADARN.RAS", "SRADARF.RAS", 36 };
	Slider *_dragging = nullptr;
	int getViewZoom() const { return 18 - _viewSlider.step(); }
	double getRadarRange() const { return (19 - _radarSlider.step()) * 7500.0 + 10; }
	void moveSlider(Slider &slider, int y);
	void drawSlider(Graphics::Surface &screen, const Slider &slider);
	double _keepRange = 0;
	bool _enemyView = false;
	bool _phasersOnline = true, _photonsOnline = true;
	uint32 _phaserLockout = 0, _photonLockout = 0;
	// Keys held: yaw, pitch, roll (-1..1) and speed changes.
	int _turnKeys[3] = {};
	int _speedKey = 0;
	int _pressed = -1;
	bool _optionsRequest = false;
	uint32 _lifeSupportOut = 0;

	Audio::SoundHandle _speech, _music, _effect;
	Common::String _subtitle;
	uint32 _subtitleLeft = 0;
	Common::String _message;
	uint32 _messageLeft = 0;
	std::map<int, uint32> _spoken;
};

} // End of namespace STG

#endif
