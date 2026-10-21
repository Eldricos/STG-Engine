/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/actors.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "common/debug.h"

namespace STG {

namespace {

const double kFixed = 65536.0;
const double kPi = 3.14159265358979323846;
const uint16 kQuarterTurn = 0x4000;
/** Actor flags (record offset 0x6C). */
const uint32 kFlagFullbright = 0x10;
const uint32 kFlagAlwaysActive = 0x400000;
/** Flyers keep their height rather than standing on the floor (0x4C3AC0). */
const uint32 kFlagFlyer = 0x1000000;
/** Sprite sequence for each animation entry (table at 0x552338). */
const int kSequenceForAnimation[LevelFile::kAnimationCount] = { 0, 2, 0, 3, 4, 5, 1 };

double fromFixed(int32 v) {
	return v / kFixed;
}

/** The original's quick distances: 2D max + 3/4 min (0x4C57F0)... */
double distance2D(double dx, double dy) {
	dx = std::fabs(dx);
	dy = std::fabs(dy);
	return std::max(dx, dy) + std::min(dx, dy) * 3 / 4;
}

/** ...and 3D max + a quarter of the other two (0x4B4630). */
double distance3D(double dx, double dy, double dz) {
	dx = std::fabs(dx);
	dy = std::fabs(dy);
	dz = std::fabs(dz);
	double m = std::max(dx, std::max(dy, dz));
	return m + (dx + dy + dz - m) / 4;
}

int animationIndex(int mode) {
	return std::min(mode, LevelFile::kAnimationCount - 1);
}

} // End of anonymous namespace

struct Actors::Actor {
	explicit Actor(const Walker &prototype) : walker(prototype) {}

	/** The record from the level, with fields the scripts change. */
	LevelFile::Actor data;
	Walker walker;
	double x = 0, y = 0, z = 0;
	double homeX = 0, homeY = 0;
	uint16 heading = 0;
	int mode = kModeIdle;
	/** Object to face, attack or walk to; 0 is the player. */
	uint16 target = 0;
	/** Animation position in 1/256 frames. */
	uint32 timer = 0;
	/** Set when a mode starts: attackers fire at once. */
	bool fresh = true;
	/** Set when a mode starts and cleared by the "now facing" report (+0x10F). */
	bool announce = true;
	// Wander and walk-to steering.
	bool turning = false;
	uint16 goal = 0;
	bool returning = false;
	double walked = 0, nextTurn = 0;
	double centreDistance = 0;
	// Proximity.
	bool near = false, inRange = false;
};

Actors::Actors(const LevelFile &level, const Walker &prototype, const SpriteLoader &loader) : _level(level) {
	for (const LevelFile::Actor &data : level.getActors()) {
		std::unique_ptr<Actor> actor(new Actor(prototype));
		actor->data = data;
		actor->heading = data.heading;
		actor->mode = data.mode;
		actor->homeX = actor->x = fromFixed(data.x);
		actor->homeY = actor->y = fromFixed(data.y);
		// The file's z includes the draw offset (0x4BE361).
		actor->z = fromFixed(data.z) - fromFixed(data.drawOffset);
		Walker &walker = actor->walker;
		walker.setRadius(fromFixed(data.radius));
		walker.setStepHeight(fromFixed(data.stepHeight));
		walker.setEyeHeight(fromFixed(data.bodyHeight));
		walker.setHeadroom(0);
		walker.setFalls(false);
		walker.setIgnoredFloors(LevelFile::kFlagNoActors);
		walker.setObstacles(&_obstacles, (int)_actors.size());
		walker.place(actor->x, actor->y, actor->z);
		if (!(data.flags & kFlagFlyer) && walker.getFloor() >= 0)
			actor->z = walker.getFeet();
		_actors.push_back(std::move(actor));

		if (loader && !_sprites.count(data.spriteSet)) {
			std::unique_ptr<SpriteSet> set(new SpriteSet());
			if (loader(data.spriteSet, *set))
				_sprites[data.spriteSet] = std::move(set);
			else
				_sprites[data.spriteSet] = nullptr;
		}
	}
	_obstacles.resize(_actors.size());
	for (size_t i = 0; i < _actors.size(); i++) {
		const Actor &a = *_actors[i];
		_obstacles[i] = { a.x, a.y, a.mode == kModeHidden ? 0 : fromFixed(a.data.radius), a.z,
		                  a.z + fromFixed(a.data.bodyHeight) };
	}
}

Actors::~Actors() {}

uint32 Actors::random() {
	// The original's generator (0x57AA68).
	_seed = _seed * 0x41C64E6D + 0x3039;
	return _seed >> 16;
}

uint16 Actors::headingTowards(double dx, double dy) {
	return (uint16)(int32)std::lround(std::atan2(-dx, dy) * 32768 / kPi);
}

bool Actors::isActor(uint16 id) const {
	return !findAll(id).empty();
}

std::vector<int> Actors::findAll(uint16 id) const {
	if ((id & 0xE000) == 0)
		id |= kObjectType;
	std::vector<int> found;
	for (size_t i = 0; i < _actors.size(); i++) {
		if ((uint16)(_actors[i]->data.objectId | kObjectType) == id)
			found.push_back((int)i);
	}
	return found;
}

uint16 Actors::getObstacleObject(int obstacle) const {
	if (obstacle < 0 || obstacle >= (int)_actors.size())
		return 0;
	return (uint16)(_actors[obstacle]->data.objectId | kObjectType);
}

void Actors::setMode(uint16 id, int mode, uint16 target) {
	for (int index : findAll(id)) {
		Actor &actor = *_actors[index];
		// Every change restarts the animation (0x4B5090).
		actor.mode = mode;
		actor.target = target;
		actor.timer = 0;
		actor.fresh = true;
		actor.announce = true;
		actor.turning = false;
		actor.returning = false;
	}
}

bool Actors::getPosition(uint16 id, double &x, double &y, double &z, uint16 &heading) const {
	std::vector<int> found = findAll(id);
	if (found.empty())
		return false;
	const Actor &actor = *_actors[found[0]];
	x = actor.x;
	y = actor.y;
	z = actor.z;
	heading = actor.heading;
	return true;
}

void Actors::setPosition(uint16 id, double x, double y, double z, uint16 heading) {
	// As 0x4B64C0: put the actor on the floor at the point.
	for (int index : findAll(id)) {
		Actor &actor = *_actors[index];
		actor.walker.place(x, y, z);
		actor.x = x;
		actor.y = y;
		bool grounded = !(actor.data.flags & kFlagFlyer) && actor.walker.getFloor() >= 0;
		actor.z = grounded ? actor.walker.getFeet() : z;
		actor.heading = heading;
	}
}

void Actors::setHeading(uint16 id, uint16 heading) {
	for (int index : findAll(id))
		_actors[index]->heading = heading;
}

void Actors::setStepHeight(uint16 id, double height) {
	for (int index : findAll(id))
		_actors[index]->walker.setStepHeight(height);
}

void Actors::setRadius(uint16 id, double radius) {
	for (int index : findAll(id)) {
		_actors[index]->data.radius = (int32)std::lround(radius * kFixed);
		_actors[index]->walker.setRadius(radius);
	}
}

void Actors::setBodyHeight(uint16 id, double height) {
	for (int index : findAll(id)) {
		_actors[index]->data.bodyHeight = (int32)std::lround(height * kFixed);
		_actors[index]->walker.setEyeHeight(height);
	}
}

void Actors::setReach(uint16 id, double reach) {
	for (int index : findAll(id))
		_actors[index]->data.reach = (int32)std::lround(reach * kFixed);
}

void Actors::setAnimationRate(uint16 id, int mode, byte rate) {
	if (mode < 0 || mode >= LevelFile::kAnimationCount)
		return;
	for (int index : findAll(id))
		_actors[index]->data.animations[mode].rate = rate;
}

void Actors::setWander(uint16 id, bool followPlayer, uint16 minTurn, uint16 maxTurn, double innerRadius,
                       double homeRadius, double outerRadius, double minWalk, double maxWalk) {
	for (int index : findAll(id)) {
		LevelFile::Actor &data = _actors[index]->data;
		data.followPlayer = followPlayer;
		// Stored as they come, as the original does: scripts give degrees
		// but the engine reads 1/65536 turns.
		data.minTurn = minTurn;
		data.maxTurn = maxTurn;
		data.innerRadius = (int32)std::lround(innerRadius * kFixed);
		data.homeRadius = (int32)std::lround(homeRadius * kFixed);
		data.outerRadius = (int32)std::lround(outerRadius * kFixed);
		data.minWalk = (int32)std::lround(minWalk * kFixed);
		data.maxWalk = (int32)std::lround(maxWalk * kFixed);
	}
}

void Actors::setProximity(int actorClass, int targetClass, const double radii[5]) {
	_proximity[(uint16)((actorClass & 0xFF) << 8 | (targetClass & 0xFF))] = std::vector<double>(radii, radii + 5);
}

bool Actors::getTarget(const Actor &actor, const Walker &player, double &x, double &y, double &z) const {
	if (actor.target != 0) {
		uint16 heading;
		if (getPosition(actor.target, x, y, z, heading))
			return true;
	}
	x = player.getX();
	y = player.getY();
	z = player.getFeet();
	return true;
}

bool Actors::turnTowards(Actor &actor, uint16 goal, uint32 ms) {
	int32 limit = (int32)((uint32)actor.data.turnSpeed * ms / 1024);
	int16 diff = (int16)(goal - actor.heading);
	if (std::abs((int)diff) <= limit) {
		actor.heading = goal;
		return true;
	}
	if (limit > 0)
		actor.heading = (uint16)(actor.heading + (diff > 0 ? limit : -limit));
	return false;
}

void Actors::moveForward(Actor &actor, double distance, uint32 ms) {
	if (distance <= 0)
		return;
	double angle = actor.heading * 2 * kPi / 65536;
	double oldX = actor.x, oldY = actor.y;
	actor.walker.move(-std::sin(angle) * distance, std::cos(angle) * distance, ms / 1000.0);
	actor.x = actor.walker.getX();
	actor.y = actor.walker.getY();
	if (!(actor.data.flags & kFlagFlyer) && actor.walker.getFloor() >= 0)
		actor.z = actor.walker.getFeet();
	actor.walked += std::sqrt((actor.x - oldX) * (actor.x - oldX) + (actor.y - oldY) * (actor.y - oldY));
}

double Actors::runAnimation(Actor &actor, uint32 ms, std::vector<ScriptEvent> &events) {
	// 0x4C4230: about `rate` frames a second. The first pass plays every
	// frame, then it loops from loopStart; loopStart = frames - 1 plays
	// once and holds the last frame.
	const LevelFile::Animation &anim = actor.data.animations[animationIndex(actor.mode)];
	uint32 span = (uint32)(anim.frames - anim.loopStart) * 256;
	uint32 delta = std::min<uint32>((uint32)anim.rate * 256 * ms >> 10, span - 1);
	actor.timer += delta;
	uint16 object = (uint16)(actor.data.objectId | kObjectType);
	bool fire = actor.mode == kModeAttack && actor.fresh;
	if ((actor.timer >> 8) >= anim.frames) {
		actor.timer -= span;
		if (anim.frames > 1)
			events.push_back({ object, kEventCycle });
		fire = actor.mode == kModeAttack;
	}
	// Attackers fire at the start of each cycle, which is reported like an
	// arrival (engine code 18). The shots themselves aren't implemented.
	if (fire)
		events.push_back({ object, kEventArrived });

	// Walking keeps pace with the animation: speed (8.8 units) per frame.
	return (double)anim.speed * delta / (256.0 * 256.0);
}

void Actors::runWander(Actor &actor, int index, double step, const Walker &player, uint32 ms) {
	// 0x4C3C10, mode 6. The centre is home, or the player.
	const LevelFile::Actor &data = actor.data;
	double cx = data.followPlayer ? player.getX() : actor.homeX;
	double cy = data.followPlayer ? player.getY() : actor.homeY;
	double distance = distance2D(cx - actor.x, cy - actor.y);
	uint16 towardsCentre = headingTowards(cx - actor.x, cy - actor.y);
	double inner = fromFixed(data.innerRadius), home = fromFixed(data.homeRadius);
	double outer = fromFixed(data.outerRadius);

	if (actor.returning) {
		// Head home until back inside the outer band.
		actor.goal = towardsCentre;
		actor.turning = true;
		if (distance < outer)
			actor.returning = false;
	} else if (home > 0 && distance > home) {
		actor.returning = true;
	} else if (outer > 0 && distance > outer && !actor.turning) {
		// Turn a quarter towards the centre.
		int16 side = (int16)(towardsCentre - actor.heading);
		actor.goal = (uint16)(actor.heading + (side >= 0 ? kQuarterTurn : -kQuarterTurn));
		actor.turning = true;
	} else if (inner > 0 && actor.centreDistance > inner && distance <= inner && !actor.turning) {
		actor.goal = (uint16)(actor.heading + kQuarterTurn);
		actor.turning = true;
	}
	actor.centreDistance = distance;

	if (!actor.turning && actor.walked >= actor.nextTurn) {
		// A random turn after a random distance, always the same way round.
		uint32 range = data.maxTurn > data.minTurn ? data.maxTurn - data.minTurn : 0;
		actor.goal = (uint16)(actor.heading + data.minTurn + (range ? random() % (range + 1) : 0));
		actor.turning = true;
		actor.walked = 0;
		double minWalk = fromFixed(data.minWalk), maxWalk = fromFixed(data.maxWalk);
		actor.nextTurn = minWalk + (maxWalk > minWalk ? (maxWalk - minWalk) * (random() % 1024) / 1023.0 : 0);
	}
	if (actor.turning && turnTowards(actor, actor.goal, ms))
		actor.turning = actor.returning;

	moveForward(actor, step, ms);
	if (actor.walker.wasBlocked()) {
		uint16 turn = kQuarterTurn + (actor.returning ? (uint16)(random() % 0x2000) : 0);
		actor.goal = (uint16)(actor.heading + turn);
		actor.turning = true;
	}
}

void Actors::runWalkTo(Actor &actor, double step, const Walker &player, uint32 ms,
                       std::vector<ScriptEvent> &events) {
	// Mode 7: straight at the target. Arrival is reported every tick while
	// within reach; the actor keeps going until a script changes its mode.
	double tx, ty, tz;
	getTarget(actor, player, tx, ty, tz);
	if (distance2D(tx - actor.x, ty - actor.y) < fromFixed(actor.data.reach))
		events.push_back({ (uint16)(actor.data.objectId | kObjectType), kEventArrived });

	uint16 aim = headingTowards(tx - actor.x, ty - actor.y);
	if (actor.turning) {
		// Sidestepping an obstacle first; there is no path finding.
		if (turnTowards(actor, actor.goal, ms))
			actor.turning = false;
	} else {
		turnTowards(actor, aim, ms);
	}
	moveForward(actor, step, ms);
	if (actor.walker.wasBlocked() && !actor.turning) {
		int16 side = (int16)(aim - actor.heading);
		actor.goal = (uint16)(actor.heading + (side >= 0 ? kQuarterTurn : -kQuarterTurn));
		actor.turning = true;
	}
}

void Actors::runProximity(Actor &actor, const Walker &player, bool frozen, std::vector<ScriptEvent> &events) {
	auto it = _proximity.find((uint16)((actor.data.spriteSet & 0xFF) << 8));
	if (it == _proximity.end())
		return;
	const std::vector<double> &r = it->second;
	uint16 object = (uint16)(actor.data.objectId | kObjectType);
	if (frozen) {
		if (actor.near)
			events.push_back({ object, kEventPlayerLost });
		actor.near = false;
		return;
	}
	// From the player's eye to the actor's feet. The original also needs a
	// line of sight for "near" (0x4BB900); that isn't checked here.
	double d = distance3D(player.getX() - actor.x, player.getY() - actor.y, player.getEye() - actor.z);
	if (!actor.near && r[1] > 0 && d < r[1]) {
		actor.near = true;
		events.push_back({ object, kEventPlayerNear });
	} else if (actor.near && d > r[2]) {
		actor.near = false;
		events.push_back({ object, kEventPlayerLost });
	}
	if (!actor.inRange && r[3] > 0 && d < r[3]) {
		actor.inRange = true;
		events.push_back({ object, kEventPlayerInRange });
	} else if (actor.inRange && d > r[4]) {
		actor.inRange = false;
		events.push_back({ object, kEventPlayerOutOfRange });
	}
}

void Actors::update(uint32 ms, const Walker &player, std::vector<ScriptEvent> &events) {
	ms = std::max<uint32>(8, std::min<uint32>(ms, 1000));
	const std::vector<LevelFile::Sector> &flats = _level.getSectors();
	for (size_t i = 0; i < _actors.size(); i++) {
		Actor &actor = *_actors[i];
		if (actor.mode == kModeHidden) {
			_obstacles[i].radius = 0;
			continue;
		}

		// Actors whose floor is in a zone the player can't see are frozen.
		int floor = actor.walker.getFloor();
		bool frozen = floor >= 0 && !(flats[floor].zones & player.getActiveZones()) &&
		              !(actor.data.flags & kFlagAlwaysActive);
		runProximity(actor, player, frozen, events);
		if (frozen)
			continue;

		double step = runAnimation(actor, ms, events);
		uint16 object = (uint16)(actor.data.objectId | kObjectType);
		switch (actor.mode) {
		case kModeFace:
		case kModeAttack: {
			double tx, ty, tz;
			getTarget(actor, player, tx, ty, tz);
			if (turnTowards(actor, headingTowards(tx - actor.x, ty - actor.y), ms) && actor.announce &&
			    actor.mode == kModeFace) {
				events.push_back({ object, kEventArrived });
				actor.announce = false;
			}
			break;
		}
		case kModeWander:
			runWander(actor, (int)i, step, player, ms);
			break;
		case kModeWalkTo:
			runWalkTo(actor, step, player, ms, events);
			break;
		default:
			break;
		}
		actor.fresh = false;

		Walker::Obstacle &o = _obstacles[i];
		o.x = actor.x;
		o.y = actor.y;
		o.radius = fromFixed(actor.data.radius);
		o.bottom = actor.z;
		o.top = actor.z + fromFixed(actor.data.bodyHeight);
	}
}

void Actors::getSprites(double cameraX, double cameraY, uint64 visibleZones, std::vector<SpriteDraw> &sprites) const {
	sprites.clear();
	const std::vector<LevelFile::Sector> &flats = _level.getSectors();
	for (const std::unique_ptr<Actor> &pointer : _actors) {
		const Actor &actor = *pointer;
		if (actor.mode == kModeHidden || (actor.data.flags & LevelFile::kFlagNotDrawn))
			continue;
		int floor = actor.walker.getFloor();
		if (floor >= 0 && !(flats[floor].zones & visibleZones))
			continue;
		auto set = _sprites.find(actor.data.spriteSet);
		if (set == _sprites.end() || !set->second)
			continue;

		// 0x4C3C10: the sequence for the mode, then the view direction from
		// the angle between the camera and the actor's heading.
		int animation = animationIndex(actor.mode);
		const LevelFile::Animation &anim = actor.data.animations[animation];
		const std::vector<std::vector<byte>> &sequences = set->second->getSequences();
		int sequence = kSequenceForAnimation[animation];
		if (sequence >= (int)sequences.size() || sequences[sequence].empty())
			continue;
		const std::vector<byte> &frames = sequences[sequence];

		uint16 seen = (uint16)(int32)std::lround(std::atan2(actor.x - cameraX, actor.y - cameraY) * 32768 / kPi);
		int octant = (uint16)(-seen - actor.heading + 0x1000) >> 13;
		int direction = octant * anim.directions >> 3;
		bool mirrored = false;
		if (anim.mirror && direction > 4) {
			direction = 8 - direction;
			mirrored = true;
		}
		int index = (byte)((actor.timer >> 8) + direction * anim.frames);
		if (index >= (int)frames.size())
			index = 0;
		int frame = frames[index];
		if (frame >= set->second->getFrameCount())
			continue;

		const SpriteSet::Frame &f = set->second->getFrame(frame);
		SpriteDraw sprite;
		sprite.x = actor.x;
		sprite.y = actor.y;
		sprite.bottom = actor.z + fromFixed(actor.data.drawOffset);
		sprite.halfWidth = f.halfWidth * fromFixed(actor.data.scale);
		sprite.height = fromFixed(actor.data.height);
		sprite.surface = &f.surface;
		sprite.mirrored = mirrored;
		// Lit like the floor underfoot (its light is copied each frame).
		sprite.light = floor >= 0 ? flats[floor].light : actor.data.light;
		sprite.fullbright = (actor.data.flags & kFlagFullbright) != 0;
		sprites.push_back(sprite);
	}
}

} // End of namespace STG
