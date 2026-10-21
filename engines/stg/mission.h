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
#include "engines/stg/level.h"
#include "engines/stg/render3d.h"
#include "engines/stg/script.h"
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
 * effects are simply missing (sound, the HUD and panels, inventory,
 * combat, screen effects).
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
		kEventUntouch = 15,
		kEventActivate = 45,
		kEventEffectDone = 55,
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

	/** actors may be null for a level without them. */
	Mission(LevelFile &level, const ScriptFile &script, LevelRenderer &renderer, Walker &walker, Actors *actors);
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

	/** Natives called that aren't implemented, by opcode, with call counts. */
	const std::map<uint16, int> &getMissingNatives() const { return _missing; }

	int16 getHealth() const { return _health; }

	// ScriptHost
	void callNative(ScriptVM &vm, ScriptInstance &self, uint16 op) override;
	void unhandledEvent(ScriptVM &vm, ScriptInstance &self, uint16 event) override;

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
	static void push32(ScriptVM &vm, double value);
	static Position popPosition(ScriptVM &vm);
	static void pushPosition(ScriptVM &vm, const Position &position);

	bool getPosition(uint16 id, Position &position) const;
	void setPosition(uint16 id, const Position &position);
	void setVisible(uint16 id, bool visible);
	void setTexture(uint16 id, uint16 texture, uint16 frames, uint16 speed);
	void restoreTexture(uint16 id);
	void setFullbright(uint16 id, bool on);
	void setPlayerHeading(int16 degrees);
	/** Set an actor's mode from a native that pops the actor's id. */
	void setActorMode(ScriptVM &vm, int mode, bool popTarget);
	void skipNative(ScriptVM &vm, uint16 op);

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
	std::multimap<uint32, uint16> _effects;
	std::vector<int16> _array;

	/** Textures as loaded, for RESET_DEFAULT. */
	std::vector<uint16> _wallTextures, _flatTextures, _billboardTextures;
	Actors *_actors;

	int16 _health = 100;
	int16 _maxHealth = 100;
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
