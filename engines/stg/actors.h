/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_ACTORS_H
#define STG_ACTORS_H

#include <climits>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "engines/stg/level.h"
#include "engines/stg/projectiles.h"
#include "engines/stg/render3d.h"
#include "engines/stg/sprite.h"
#include "engines/stg/walker.h"

namespace STG {

/**
 * The actors of a level (GAME.LV2 type 6): their AI, movement, animation
 * and sprites, after the original's actor update (0x4C3C10) and animation
 * timer (0x4C4230).
 *
 * Each actor is in one of these modes, which scripts set:
 *
 * | Mode | Behaviour | Animation |
 * |------|-----------|-----------|
 * | 0 idle | stands | stand (sequence 0) |
 * | 1 | stands | pain (sequence 2) |
 * | 2 face | turns to face its target or the player; event 36 when facing | stand |
 * | 3 attack | as 2, and fires at the start of each animation cycle | attack (3) |
 * | 4, 5 | stands | deaths (4, 5) |
 * | 6 wander | walks around its home or the player, turning at random | walk (1) |
 * | 7 walk to | walks straight at its target or the player; event 36 while within reach | walk (1) |
 * | 8 hidden | removed from the level | |
 *
 * Every animation cycle ends with event 37. Nothing changes mode by itself;
 * scripts react to the events. Actors move with the player's collision
 * (walls, other actors, step height) and snap onto floors rather than fall.
 */
class Actors {
public:
	enum Mode {
		kModeIdle = 0,
		kModePain = 1,
		kModeFace = 2,
		kModeAttack = 3,
		kModeDeathA = 4,
		kModeDeathB = 5,
		kModeWander = 6,
		kModeWalkTo = 7,
		kModeHidden = 8
	};

	/** Script events actors raise, sent to the actor's object. */
	enum Event {
		kEventArrived = 36,
		kEventCycle = 37,
		kEventPlayerNear = 29,
		kEventPlayerLost = 31,
		kEventPlayerInRange = 33,
		kEventPlayerOutOfRange = 35,
		// The outcome of a hit (0x45747E).
		kEventStunned = 38,
		kEventGrazed = 39,
		kEventHurt = 40,
		kEventKnockedOut = 41,
		kEventKilled = 42
	};

	/**
	 * An actor object's toughness (SET_ACTOR_COMBAT_STATS). A hit with
	 * power above fullThreshold takes damage from both hit points and stun
	 * points; above stunThreshold from stun points only; above
	 * noticeThreshold it is only noticed.
	 */
	struct CombatStats {
		int32 fullThreshold = INT32_MAX;
		int32 stunThreshold = INT32_MAX;
		int32 noticeThreshold = INT32_MAX;
		int32 stunMax = 0;
		int32 stun = 0;
		int32 hitPointsMax = 0;
		int32 hitPoints = 0;
	};
	/** In SET_ACTOR_COMBAT_STATS, this keeps a value as it is. */
	static const int32 kKeep = 0x8000;

	struct ScriptEvent {
		/** The actor's script object id (id | 0xA000). */
		uint16 object;
		uint16 event;
	};

	/** Loads sprite set NNN.gs2; returns false if it can't. */
	typedef std::function<bool(uint16 number, SpriteSet &set)> SpriteLoader;

	static const uint16 kObjectType = 0xA000;

	/** Places every actor on its floor and loads the sprite sets they use. */
	Actors(const LevelFile &level, const Walker &prototype, const SpriteLoader &loader);
	~Actors();

	int getCount() const { return (int)_actors.size(); }

	/**
	 * Advance by ms milliseconds (the original clamps this to 8-1000): run
	 * each actor's mode and animation, and collect the events raised. player
	 * is the player's walker; actors standing in zones it can't see are
	 * frozen, as in the original.
	 */
	void update(uint32 ms, const Walker &player, std::vector<ScriptEvent> &events);

	/** Actors as obstacles, for the player's walker. Updated in place. */
	const std::vector<Walker::Obstacle> &getObstacles() const { return _obstacles; }
	/** The script object of the actor behind an obstacle. */
	uint16 getObstacleObject(int obstacle) const;

	/** The sprites to draw, seen from (cameraX, cameraY). */
	void getSprites(double cameraX, double cameraY, uint64 visibleZones, std::vector<SpriteDraw> &sprites) const;

	// Script control. id is an actor's script object id; ids without type
	// bits are taken as actor ids, as the original's lookup (game.vt[0x15C]) does.
	bool isActor(uint16 id) const;
	/** Set the mode; target is the object to face, attack or walk to (0 = the player). */
	void setMode(uint16 id, int mode, uint16 target = 0);
	/** Position in world units and heading in 1/65536 turns. */
	bool getPosition(uint16 id, double &x, double &y, double &z, uint16 &heading) const;
	void setPosition(uint16 id, double x, double y, double z, uint16 heading);
	void setHeading(uint16 id, uint16 heading);
	void setStepHeight(uint16 id, double height);
	void setRadius(uint16 id, double radius);
	void setBodyHeight(uint16 id, double height);
	void setReach(uint16 id, double reach);
	void setAnimationRate(uint16 id, int mode, byte rate);
	/** Wander settings (SET_ACTOR_COMBAT_PARAMS). */
	void setWander(uint16 id, bool followPlayer, uint16 minTurn, uint16 maxTurn, double innerRadius,
	               double homeRadius, double outerRadius, double minWalk, double maxWalk);
	/**
	 * Distances for the proximity events between actors of class
	 * actorClass and the player (targetClass 0): radii[1] near, [2] lost,
	 * [3] in range, [4] out of range. radii[0] is unused, as in the original.
	 */
	void setProximity(int actorClass, int targetClass, const double radii[5]);

	/** Set the stats; values of kKeep leave a stat as it is. Order as CombatStats. */
	void setCombatStats(uint16 id, const int32 values[7]);
	CombatStats getCombatStats(uint16 id) const;
	/** Actors on the same team don't hurt each other. */
	void setTeam(uint16 id, int16 team);
	int16 getTeam(uint16 id) const;
	/**
	 * Take a hit (0x45747E): returns the event the actor's object gets
	 * (kEventHurt and so on), or 0 if it doesn't notice.
	 */
	uint16 takeHit(uint16 id, int damage, int power);
	/** The radius it collides and is hit with. */
	double getRadius(uint16 id) const;
	/** The projectile type it fires in attack mode, or -1 for none. */
	void setWeapon(uint16 id, int weapon);

	/** A shot an actor fired: where from and which way (radians, as Projectiles::fire). */
	struct Shot {
		uint16 shooter;
		int weapon;
		double x, y, z, yaw, pitch;
	};
	/** Move the shots fired since the last call into shots. */
	void takeShots(std::vector<Shot> &shots);
	/** The actors, as things shots can hit. */
	void getTargets(std::vector<Projectiles::Target> &targets) const;

	/** Headings: 65536 is a full turn, 0 faces +y and 16384 faces -x. */
	static uint16 headingTowards(double dx, double dy);

private:
	struct Actor;

	std::vector<int> findAll(uint16 id) const;
	bool getTarget(const Actor &actor, const Walker &player, double &x, double &y, double &z) const;
	/** Advance the animation; returns the distance walked with it. */
	double runAnimation(Actor &actor, uint32 ms, std::vector<ScriptEvent> &events);
	void runProximity(Actor &actor, const Walker &player, bool frozen, std::vector<ScriptEvent> &events);
	void runWander(Actor &actor, int index, double step, const Walker &player, uint32 ms);
	void runWalkTo(Actor &actor, double step, const Walker &player, uint32 ms, std::vector<ScriptEvent> &events);
	void moveForward(Actor &actor, double distance, uint32 ms);
	bool turnTowards(Actor &actor, uint16 goal, uint32 ms);
	Shot aim(const Actor &actor, const Walker &player) const;
	uint32 random();

	const LevelFile &_level;
	std::vector<std::unique_ptr<Actor>> _actors;
	std::map<uint16, std::unique_ptr<SpriteSet>> _sprites;
	std::vector<Walker::Obstacle> _obstacles;
	/** (actor class << 8 | target class) -> radii. */
	std::map<uint16, std::vector<double>> _proximity;
	/** By script object id: stats belong to the object, not its parts. */
	std::map<uint16, CombatStats> _combat;
	std::map<uint16, int16> _teams;
	std::vector<Shot> _shots;
	uint32 _seed = 1;
};

} // End of namespace STG

#endif
