/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_WALKER_H
#define STG_WALKER_H

#include <vector>

#include "engines/stg/level.h"

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
	static constexpr double kEyeHeight = 6.0;
	static constexpr double kHeadroom = 0.51;
	static constexpr double kStepHeight = 2.0;
	static constexpr double kRadius = 1.2;
	/** Units per second squared; the original's falling is not decoded yet. */
	static constexpr double kGravity = 50.0;

	static const uint64 kAllZones = ~(uint64)0;

	explicit Walker(const LevelFile &level);

	/** Put the player's feet at (x, y, feet) and settle onto the floor there. */
	void place(double x, double y, double feet);

	/** Walk by (dx, dy) over the given time, colliding and following floors. */
	void move(double dx, double dy, double seconds);

	/** With collision off the player walks through walls but keeps to floors. */
	void setCollisionEnabled(bool enabled) { _collision = enabled; }
	bool isCollisionEnabled() const { return _collision; }

	double getX() const { return _x; }
	double getY() const { return _y; }
	double getFeet() const { return _feet; }
	double getEye() const { return _feet + kEyeHeight; }
	uint64 getActiveZones() const { return _zones; }
	/** Index of the flat underfoot, or -1. */
	int getFloor() const { return _floor; }

	/**
	 * The highest floor under (x, y) in the given zones that is no higher
	 * than maxHeight, or -1.
	 */
	int findFloor(double x, double y, double maxHeight, uint64 zones) const;

	struct Hit {
		/** Fraction of the move before touching. */
		double t;
		/** Unit direction along the obstacle, to slide along. */
		double slideX, slideY;
		int wall;
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
	};

	void collideAndSlide(double dx, double dy);
	void followFloor(double seconds);

	const LevelFile &_level;
	std::vector<WallLine> _walls;
	double _x = 0, _y = 0, _feet = 0;
	double _fallSpeed = 0;
	int _floor = -1;
	uint64 _zones = kAllZones;
	bool _collision = true;
};

} // End of namespace STG

#endif
