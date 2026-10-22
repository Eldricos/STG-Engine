/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_WALKER_H
#define STG_WALKER_H

#include <memory>
#include <vector>

#include "engines/stg/level.h"

namespace Common {
class Serializer;
}

namespace STG {

/**
 * Moves the player through a level the way the original does (0x4B46A0):
 *
 * - The player is a cylinder of radius 1.2 whose feet are 6 units below
 *   the eye, reaching 0.51 above it. Walls lower than 2 units above the
 *   feet are stepped over.
 * - A move is swept against wall lines and wall ends (0x4B9940). On a hit
 *   the player stops at the wall and slides along it with what is left of
 *   the move, once; a second hit ends the move there (0x4B9870).
 * - A wall blocks only from its front, the side its normal points to,
 *   unless it is flagged solid on both sides.
 * - The player stands on the highest floor no more than the step height
 *   above the feet, and falls when there is none below.
 * - The floor stood on decides which zones are active. Only walls and
 *   floors in those zones count, and the renderer only draws them. With
 *   no floor underfoot, every zone is active.
 *
 * Distances are in world units (not fixed point).
 */
class Walker {
public:
	/** The original's defaults (0x4BE150); scripts can change them. */
	static constexpr double kEyeHeight = 6.0;
	static constexpr double kHeadroom = 0.51;
	static constexpr double kStepHeight = 2.0;
	static constexpr double kRadius = 1.2;
	/** How much slimmer than the player wall ends are as posts. */
	static constexpr double kPostSlack = 0.1;
	/** Units per second squared; the original's falling is not decoded yet. */
	/** The original's defaults (game+0x10C): 64.4 (2 x 32.2) and 200. */
	static constexpr double kGravity = 64.4;
	static constexpr double kMaxFallSpeed = 200.0;

	static const uint64 kAllZones = ~(uint64)0;

	explicit Walker(const LevelFile &level);
	/** Copies share the level's wall lines, so walkers for actors are cheap. */
	Walker(const Walker &other) = default;

	/** Put the player's feet at (x, y, feet) and settle onto the floor there. */
	void place(double x, double y, double feet);
	/** Walls a door moved: take their new places, for this walker and every copy of it. */
	void refreshWalls(const std::vector<int> &walls);
	/** Save or load the position and the settings scripts change. */
	void sync(Common::Serializer &s);

	/** Walk by (dx, dy) over the given time, colliding and following floors. */
	void move(double dx, double dy, double seconds);

	/** A round obstacle, such as an actor. */
	struct Obstacle {
		double x, y, radius;
		double bottom, top;
	};

	/**
	 * Obstacles to collide with besides walls; the vector must outlive the
	 * walker's moves. ignore is an index to skip (the mover itself), or -1.
	 */
	void setObstacles(const std::vector<Obstacle> *obstacles, int ignore = -1) {
		_obstacles = obstacles;
		_ignoredObstacle = ignore;
	}

	/** Without falling, the walker drops straight onto lower floors, as actors do. */
	void setFalls(bool falls) { _falls = falls; }
	/** Gravity in units/s/s and the fastest fall in units/s (SET_GRAVITY, LOW_GRAVITY). */
	void setGravity(double gravity, double maxFallSpeed) {
		_gravity = gravity;
		_maxFallSpeed = maxFallSpeed;
	}
	double getGravity() const { return _gravity; }
	double getMaxFallSpeed() const { return _maxFallSpeed; }
	/** How fast a fall hit the floor since the last call, or 0 (for fall damage). */
	double takeLanding() {
		double speed = _landingSpeed;
		_landingSpeed = 0;
		return speed;
	}

	/** Floors with any of these flags are ignored, e.g. LevelFile::kFlagNoActors. */
	void setIgnoredFloors(uint32 flags) { _ignoredFloors = flags; }

	/** With collision off the player walks through walls but keeps to floors. */
	void setCollisionEnabled(bool enabled) { _collision = enabled; }
	bool isCollisionEnabled() const { return _collision; }

	double getX() const { return _x; }
	double getY() const { return _y; }
	double getFeet() const { return _feet; }
	double getEye() const { return _feet + _eyeHeight; }

	void setEyeHeight(double height) { _eyeHeight = height; }
	void setStepHeight(double height) { _stepHeight = height; }
	void setHeadroom(double height) { _headroom = height; }
	void setRadius(double radius) { _radius = radius; }
	double getRadius() const { return _radius; }

	/**
	 * The wall (index into the level's walls) the last move ran into, or
	 * -1. Like the original, standing still touches nothing.
	 */
	int getContactWall() const { return _contactWall; }
	/** The obstacle the last move ran into, or -1. */
	int getContactObstacle() const { return _contactObstacle; }
	/** Whether the last move was stopped short by anything. */
	bool wasBlocked() const { return _contactWall >= 0 || _contactObstacle >= 0; }
	uint64 getActiveZones() const { return _zones; }
	/** Index of the flat underfoot, or -1. */
	int getFloor() const { return _floor; }

	/**
	 * The highest floor under (x, y) in the given zones that is no higher
	 * than maxHeight, or -1.
	 */
	int findFloor(double x, double y, double maxHeight, uint64 zones) const {
		return findFloor(x, y, maxHeight, zones, _ignoredFloors);
	}
	/** The same, skipping floors with any of ignoredFlags instead of setIgnoredFloors()'s. */
	int findFloor(double x, double y, double maxHeight, uint64 zones, uint32 ignoredFlags) const;

	/**
	 * Whether nothing blocks the view from a to b, as the original's test
	 * for actors seeing the player (0x4BB900): visible walls and floors in
	 * the given zones block, with a quarter unit of slack on walls; nothing
	 * steeper than twice as high as it is long is ever seen.
	 */
	bool lineOfSight(double ax, double ay, double az, double bx, double by, double bz, uint64 zones) const;

	struct Hit {
		/** Fraction of the move before touching. */
		double t;
		/** Unit direction along the obstacle, to slide along. */
		double slideX, slideY;
		/** Index into the level's walls, or -1. */
		int wall;
		/** Index into the obstacles, or -1. */
		int obstacle;
	};

	/**
	 * Sweep the player from (x, y) by (dx, dy) and report the first wall
	 * touched. Returns false if the way is clear.
	 */
	bool sweep(double x, double y, double dx, double dy, Hit &hit) const;

private:
	struct WallLine {
		double x1, y1, x2, y2;
		double nx, ny, d;
		double top, bottom;
		uint64 zones;
		bool bothSides;
		int index;
	};

	void makeLine(int index, WallLine &line) const;
	void collideAndSlide(double dx, double dy);
	void followFloor(double seconds);

	const LevelFile &_level;
	std::shared_ptr<std::vector<WallLine>> _walls;
	double _x = 0, _y = 0, _feet = 0;
	double _fallSpeed = 0;
	double _gravity = kGravity, _maxFallSpeed = kMaxFallSpeed;
	double _landingSpeed = 0;
	int _floor = -1;
	int _contactWall = -1;
	int _contactObstacle = -1;
	const std::vector<Obstacle> *_obstacles = nullptr;
	int _ignoredObstacle = -1;
	bool _falls = true;
	uint32 _ignoredFloors = 0;
	double _eyeHeight = kEyeHeight;
	double _stepHeight = kStepHeight;
	double _headroom = kHeadroom;
	double _radius = kRadius;
	uint64 _zones = kAllZones;
	bool _collision = true;
};

} // End of namespace STG

#endif
