/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_MISSION_H
#define STG_MISSION_H

#include <map>
#include <memory>
#include <vector>

#include "common/random.h"
#include "engines/stg/actors.h"
#include "engines/stg/hud.h"
#include "engines/stg/level.h"
#include "engines/stg/projectiles.h"
#include "engines/stg/render3d.h"
#include "engines/stg/script.h"
#include "engines/stg/sound.h"
#include "engines/stg/vm.h"
#include "engines/stg/walker.h"

namespace STG {

/**
 * Runs a 3D level's scripts (GAME.BIN) against the level (GAME.LV2): the
 * role of the original's mission object (vtable 0x531768).
 *
 * At the start every script object enters state 0, in the order the
 * original visits them, and the "mission started" broadcast follows. After
 * that, scripts run when something happens:
 *
 * - **Timers** send event 1 to their object.
 * - **The player** touching a wall, stepping onto a floor or entering a
 *   billboard's area sends events to that thing's script object. The
 *   object's id is the record's id plus the type bits: 0x4000 walls,
 *   0xC000 floors, 0x2000 billboards, 0xA000 actors.
 * - **Scripts** signal each other.
 *
 * The natives the scripts call are implemented here as far as they are
 * understood. The rest balance the stack and are counted, so their
 * effects are simply missing (the HUD and panels, inventory, combat,
 * screen effects).
 */
class Mission : public ScriptHost {
public:
	/** Script events, as the original numbers them. */
	enum Event {
		kEventTimer = 1,
		kEventSignal = 4,
		kEventSetValue = 5,
		kEventStart = 6,
		kEventPlayerDamaged = 9,
		kEventPlayerHealed = 10,
		kEventTouch = 14,
		/** Something hit the object: damage, power, the attacker's team. */
		kEventHit = 16,
		kEventUntouch = 15,
		kEventActivate = 45,
		kEventEffectDone = 55,
		kEventHitReport = 59,
		/** Broadcast when the player fires a weapon: its cost. */
		kEventItemUsed = 50,
		kEventFloorLeft = 54,
		kEventFloorEntered = 53
	};

	/** Type bits that turn a level record's id into its script object's id. */
	enum ObjectType {
		kTypeBillboard = 0x2000,
		kTypeWall = 0x4000,
		kTypeActor = 0xA000,
		kTypeFlat = 0xC000,
		kTypeLogic = 0xE000,
		kTypeMask = 0xE000
	};

	/**
	 * actors may be null for a level without them, and mixer null to run
	 * silently; soundFiles opens the sound files the scripts play.
	 */
	Mission(LevelFile &level, const ScriptFile &script, LevelRenderer &renderer, Walker &walker, Actors *actors,
	        Audio::Mixer *mixer = nullptr, const MissionSound::FileOpener &soundFiles = nullptr);
	~Mission() override;

	/** Start the level: every object to state 0, then the start broadcast. */
	void start();

	/**
	 * Advance the mission by ms milliseconds: run due timers and tell the
	 * scripts what the player's last move ran into. Call after moving the
	 * walker.
	 */
	void update(uint32 ms);

	/** Milliseconds since the mission started. */
	uint32 getTime() const { return _time; }

	/** If a script turned the player since the last call, the new yaw. */
	bool takePlayerYaw(double &yaw);
	/** The camera's yaw, for scripts that ask which way the player faces. */
	void setPlayerYaw(double yaw) { _currentYaw = yaw; }

	ScriptVM &getVM() { return *_vm; }
	MissionSound &getSound() { return *_sound; }

	/** A subtitle that started since the last call, and how long to show it. */
	bool takeSubtitle(Common::String &text, uint32 &durationMs) { return _sound->takeSubtitle(text, durationMs); }

	/** Natives called that aren't implemented, by opcode, with call counts. */
	const std::map<uint16, int> &getMissingNatives() const { return _missing; }

	int16 getHealth() const { return _health; }
	/** Hurt the player (game.vt34C): source object, damage, and a kind (a shot's team). */
	void damagePlayer(int16 source, int16 damage, int16 kind);

	/**
	 * The bare-hand attack (SET_PUNCH): an overlay played over the view, a
	 * frame at which it lands, what it does, and its sounds.
	 */
	struct Punch {
		/** Overlay file name, from the script's resources; empty for none. */
		Common::String image;
		uint32 msPerFrame = 100;
		int hitFrame = 0;
		int16 damage = 0, power = 0;
		int swingSound = -1, swingVolume = -1;
		int hitSound = -1, hitVolume = -1;
	};
	const Punch &getPunch() const { return _punch; }
	/** The punch starts: its swing sounds. */
	void swing();
	/** The punch lands on what the view shows at its centre (code 19, 0x4364B8). */
	void punch(const LevelRenderer::Pick &pick);

	/**
	 * An inventory item's behaviour (ATTACH_ITEM_BEHAVIOUR). Weapons and
	 * the like hold a charge (0x449DCD); each use costs the power level,
	 * which is also the damage of what they fire.
	 */
	struct Item {
		int16 kind = 0, b = 0, c = 0;
		int32 charge = 0, maxCharge = 0;
		int32 level = 0, maxLevel = 0;
		/** SET_ITEM_SETTING: how long the muzzle flashes, in ms. */
		int32 setting = 0;
	};
	/** An item's behaviour, or null. */
	const Item *getItem(uint16 id) const {
		auto it = _items.find(id);
		return it == _items.end() ? nullptr : &it->second;
	}
	/** Set the active item's power level (the [ and ] keys, the weapon display), within 1 and its maximum. */
	void setItemLevel(int level);

	enum FireResult {
		kNoWeapon, ///< nothing to fire: the player punches instead
		kEmpty,    ///< not enough charge
		kFired
	};
	/**
	 * Fire the active item if it is a weapon (0x43FDC4) from its muzzle
	 * (ahead, up and to the left of the eye) at what the view shows at
	 * (x, y): pick, the renderer's pick there, in a view w x h seen
	 * through camera.
	 */
	FireResult fireWeapon(const Camera &camera, const LevelRenderer::Pick &pick, int x, int y, int w, int h,
	                      double ahead, double up, double left);

	/** The console: inventory, panels, body picture, map markers, screen effects. */
	Hud &getHud() { return _hud; }
	/** Use the item in an inventory slot, as clicking it does (event 45). */
	void useItem(int slot);
	/** Right click on an inventory slot (event 62). */
	void describeItem(int slot);
	/** An item dragged out of the inventory (event 63). */
	void dropItem(int slot);
	/** Whether an item is one the player holds and fires. */
	bool isWeapon(uint16 id) const;
	/** An item's kind (ATTACH_ITEM_BEHAVIOUR), or 0. */
	int getItemKind(uint16 id) const {
		auto it = _items.find(id);
		return it == _items.end() ? 0 : it->second.kind;
	}
	/** Where an object (the player for 0) is. */
	bool locate(uint16 id, double &x, double &y, double &z) const {
		Position position;
		if (!getPosition(id, position))
			return false;
		x = position.x;
		y = position.y;
		z = position.z;
		return true;
	}
	/** An item's frame in the mission's INVENT.BMP, or -1. */
	int getItemIcon(uint16 id) const;
	/** GAME_MODE 2 or 3 asked for the left panel off or on its body view since the last call; 0 if not. */
	int takeLeftPanelRequest() {
		int request = _leftPanelRequest;
		_leftPanelRequest = 0;
		return request;
	}
	/** END_MISSION (0xBF) ran: the location is over. */
	bool isFinished() const { return _finished; }
	/** What the scripts left for the game: a value (0x99, -1 if unset) and flags (0x9A). */
	int32 getResult() const { return _result; }
	uint32 getResultFlags() const { return _resultFlags; }

	/** The item in use (SET_ACTIVE_ITEM), or 0. */
	uint16 getActiveItem() const { return _activeItem; }

	/** Fire a projectile of the given type (see Projectiles::fire), sounding it. */
	void fire(int type, double x, double y, double z, double yaw, double pitch, int shooter);
	Projectiles &getProjectiles() { return *_projectiles; }

	// ScriptHost
	void callNative(ScriptVM &vm, ScriptInstance &self, uint16 op) override;
	void unhandledEvent(ScriptVM &vm, ScriptInstance &self, uint16 event, const std::vector<int16> &args) override;

	/** Send an event to the object with the given id, if there is one. */
	int16 send(uint16 id, uint16 event, const std::vector<int16> &args = {});
	/** Send an event to every object that subscribed to broadcasts. */
	void broadcast(uint16 event, const std::vector<int16> &args = {});

private:
	struct Timer {
		uint32 period;
		uint32 due;
	};

	struct Position {
		double x, y, z;
		int16 heading;
	};

	/** Object order at the start, as the original's hash map yields it. */
	std::vector<int> getStartOrder() const;

	void runTimers();
	void checkPlayerContacts();
	void setTimer(uint16 id, uint32 ms);

	/** Pop a 16.16 dword (high word on top). */
	static double pop32(ScriptVM &vm);
	/** Pop a 32-bit integer (high word on top). */
	static uint32 popRaw32(ScriptVM &vm);
	static void push32(ScriptVM &vm, double value);
	static Position popPosition(ScriptVM &vm);
	static void pushPosition(ScriptVM &vm, const Position &position);

	bool getPosition(uint16 id, Position &position) const;
	/** SET_POSITION: actors move; billboards stay put (items settle onto the floor). */
	void setPosition(uint16 id, const Position &position);
	/** DROP_AT: move an actor or billboard to a position, billboards onto the floor there. */
	void dropAt(uint16 id, const Position &position);
	void dropBillboard(LevelFile::Billboard &billboard);
	void setVisible(uint16 id, bool visible);
	void setTexture(uint16 id, uint16 texture, uint16 frames, uint16 speed);
	void restoreTexture(uint16 id);
	void setFullbright(uint16 id, bool on);
	void setPlayerHeading(int16 degrees);
	/** Set an actor's mode from a native that pops the actor's id. */
	void setActorMode(ScriptVM &vm, int mode, bool popTarget);
	void skipNative(ScriptVM &vm, uint16 op);
	void runProjectiles(uint32 ms);
	void playSoundAt(int sound, double x, double y, double z);

	LevelFile &_level;
	const ScriptFile &_script;
	LevelRenderer &_renderer;
	Walker &_walker;
	std::unique_ptr<ScriptVM> _vm;
	Common::RandomSource _random;

	uint32 _time = 0;
	std::map<uint16, Timer> _timers;
	/** Objects that receive broadcasts, newest first. */
	std::vector<uint16> _listeners;
	/** Screen effects still running: end time -> object to tell. */
	std::vector<int16> _array;

	/** Textures as loaded, for RESET_DEFAULT. */
	std::vector<uint16> _wallTextures, _flatTextures, _billboardTextures;
	Actors *_actors;
	std::unique_ptr<MissionSound> _sound;
	std::unique_ptr<Projectiles> _projectiles;

	/** An item's behaviour (ATTACH_ITEM_BEHAVIOUR): its kind and two settings. */
	std::map<uint16, Item> _items;
	Hud _hud;
	int _leftPanelRequest = 0;
	uint16 _activeItem = 0;
	bool _finished = false;
	int32 _result = -1;
	uint32 _resultFlags = 0;
	Punch _punch;
	std::vector<MissionSound::Finished> _soundsFinished;

	// The original takes the maximum from a character record not yet
	// decoded; its health bar holds about 54 units, so 50 is assumed.
	int16 _health = 50;
	int16 _maxHealth = 50;
	int _touchedWall = -1;
	int _standingFlat = -1;
	int _billboard = -1;
	uint16 _touchedActor = 0;
	std::vector<Actors::ScriptEvent> _actorEvents;
	bool _hasYaw = false;
	double _yaw = 0;
	double _currentYaw = 0;

	std::map<uint16, int> _missing;
};

} // End of namespace STG

#endif
