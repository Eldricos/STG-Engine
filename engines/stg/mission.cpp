/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/mission.h"

#include <algorithm>
#include <cmath>

#include "common/debug.h"
#include "engines/stg/opcodes.h"

namespace STG {

namespace {

const char *const kDebugChannel = "scripts";
const double kPi = 3.14159265358979323846;
const double kFixed = 65536.0;
/** Size of the second word array (F2/F3); the original's bound isn't known. */
const int kArraySize = 4096;
/** Most timer firings handled for one timer in one update, after a long pause. */
const int kMaxTimerCatchUp = 4;

double fromFixed(int32 v) {
	return v / kFixed;
}

int32 toFixed(double v) {
	return (int32)std::lround(v * kFixed);
}

/** Whether a record whose id field is recordId belongs to script object id. */
bool isObject(uint16 recordId, uint16 typeBits, uint16 id) {
	return recordId != 0 && (uint16)(recordId | typeBits) == id;
}

} // End of anonymous namespace

Mission::Mission(LevelFile &level, const ScriptFile &script, LevelRenderer &renderer, Walker &walker, Actors *actors,
                 Audio::Mixer *mixer, const MissionSound::FileOpener &soundFiles)
	: _level(level), _script(script), _renderer(renderer), _walker(walker), _random(0x53544721), _actors(actors) {
	// Sounds that follow a billboard (mode 2) or an actor (mode 3).
	auto locate = [this](int mode, uint16 object, double &x, double &y, double &z) {
		Position position;
		uint16 id = (uint16)(object | (mode == MissionSound::kPositionActor ? kTypeActor : kTypeBillboard));
		if (!getPosition(id, position))
			return false;
		x = position.x;
		y = position.y;
		z = position.z;
		return true;
	};
	_sound.reset(new MissionSound(script, mixer, soundFiles, locate));
	_vm.reset(new ScriptVM(script, this));
	_array.assign(kArraySize, 0);
	for (const LevelFile::Wall &wall : level.getWalls())
		_wallTextures.push_back(wall.texture);
	for (const LevelFile::Sector &flat : level.getSectors())
		_flatTextures.push_back(flat.texture);
	for (const LevelFile::Billboard &billboard : level.getBillboards())
		_billboardTextures.push_back(billboard.texture);
}

Mission::~Mission() {}

std::vector<int> Mission::getStartOrder() const {
	// The original keeps its objects in an MFC CMap with 17 buckets and the
	// default hash (key >> 4), inserting at the head of each bucket, and
	// visits buckets in order (0x4356FF).
	std::vector<int> order;
	for (int i = 0; i < _vm->getInstanceCount(); i++)
		order.push_back(i);
	std::stable_sort(order.begin(), order.end(), [this](int a, int b) {
		uint16 ida = _vm->getInstance(a).object->id, idb = _vm->getInstance(b).object->id;
		int ba = (ida >> 4) % 17, bb = (idb >> 4) % 17;
		return ba != bb ? ba < bb : a > b;
	});
	return order;
}

void Mission::start() {
	_listeners.clear();
	for (int index : getStartOrder())
		_vm->setState(_vm->getInstance(index), 0);
	// The original broadcasts event 6 from its mission object's method
	// 0x43E742, which also marks the mission as begun. verid1 places the
	// player at the start when it arrives, so it is sent once here.
	broadcast(kEventStart);
	checkPlayerContacts();
}

void Mission::update(uint32 ms) {
	// The original clamps its frame time the same way (0x4309C5).
	ms = std::max<uint32>(8, std::min<uint32>(ms, 1000));
	_time += ms;
	runTimers();

	if (_actors) {
		_actorEvents.clear();
		_actors->update(ms, _walker, _actorEvents);
		for (const Actors::ScriptEvent &event : _actorEvents)
			send(event.object, event.event);
	}

	while (!_effects.empty() && _effects.begin()->first <= _time) {
		uint16 id = _effects.begin()->second;
		_effects.erase(_effects.begin());
		send(id, kEventEffectDone);
	}

	checkPlayerContacts();

	MissionSound::Listener listener = { _walker.getX(), _walker.getY(), _walker.getEye(), _currentYaw };
	_soundsFinished.clear();
	_sound->update(ms, listener, _soundsFinished);
	for (const MissionSound::Finished &done : _soundsFinished)
		send(done.object, MissionSound::kEventFinished);
}

bool Mission::takePlayerYaw(double &yaw) {
	if (!_hasYaw)
		return false;
	yaw = _yaw;
	_hasYaw = false;
	return true;
}

int16 Mission::send(uint16 id, uint16 event, const std::vector<int16> &args) {
	int index = _vm->findInstance(id);
	if (index < 0)
		return 0;
	debugC(2, kDebugChannel, "event %u to %04X", event, id);
	return _vm->sendEvent(_vm->getInstance(index), event, args);
}

void Mission::broadcast(uint16 event, const std::vector<int16> &args) {
	// Copied: handlers may subscribe or unsubscribe.
	std::vector<uint16> listeners = _listeners;
	for (uint16 id : listeners)
		send(id, event, args);
}

void Mission::setTimer(uint16 id, uint32 ms) {
	Timer timer;
	timer.period = std::max<uint32>(ms, 1);
	timer.due = _time + timer.period;
	_timers[id] = timer;
}

void Mission::runTimers() {
	for (int round = 0; round < kMaxTimerCatchUp; round++) {
		std::vector<std::pair<uint32, uint16>> due;
		for (const auto &timer : _timers) {
			if (timer.second.due <= _time)
				due.push_back({ timer.second.due, timer.first });
		}
		if (due.empty())
			return;
		std::sort(due.begin(), due.end());
		for (const auto &entry : due) {
			auto it = _timers.find(entry.second);
			if (it == _timers.end() || it->second.due > _time)
				continue;
			// Periodic until killed, as with the original's Windows timers.
			it->second.due += it->second.period;
			if (_vm->findInstance(entry.second) < 0) {
				_timers.erase(it);
				continue;
			}
			send(entry.second, kEventTimer);
		}
	}
	// Still behind (a long pause): drop the missed firings.
	for (auto &timer : _timers) {
		if (timer.second.due <= _time)
			timer.second.due = _time + timer.second.period;
	}
}

void Mission::checkPlayerContacts() {
	// The engine reports only changes (0x4B46A0, 0x435CD8): the old thing
	// gets "untouch" and the new one "touch".
	int wall = _walker.getContactWall();
	if (wall != _touchedWall) {
		if (_touchedWall >= 0)
			send((uint16)(_level.getWalls()[_touchedWall].objectId | kTypeWall), kEventUntouch);
		_touchedWall = wall;
		if (wall >= 0)
			send((uint16)(_level.getWalls()[wall].objectId | kTypeWall), kEventTouch);
	}

	uint16 actor = _actors ? _actors->getObstacleObject(_walker.getContactObstacle()) : 0;
	if (actor != _touchedActor) {
		if (_touchedActor)
			send(_touchedActor, kEventUntouch);
		_touchedActor = actor;
		if (actor)
			send(actor, kEventTouch);
	}

	int flat = _walker.getFloor();
	if (flat != _standingFlat) {
		if (_standingFlat >= 0) {
			uint16 id = (uint16)(_level.getSectors()[_standingFlat].objectId | kTypeFlat);
			send(id, kEventUntouch);
			send(id, kEventFloorLeft);
		}
		_standingFlat = flat;
		if (flat >= 0) {
			uint16 id = (uint16)(_level.getSectors()[flat].objectId | kTypeFlat);
			send(id, kEventFloorEntered);
			send(id, kEventTouch);
		}
	}

	// Billboards fire when the player enters the box around them (0x4B9940).
	int inside = -1;
	const std::vector<LevelFile::Billboard> &billboards = _level.getBillboards();
	double feet = _walker.getFeet(), head = _walker.getEye() + Walker::kHeadroom;
	for (size_t i = 0; i < billboards.size() && inside < 0; i++) {
		const LevelFile::Billboard &b = billboards[i];
		if (b.radius <= 0 || b.hidden || !(b.zones & _walker.getActiveZones()))
			continue;
		double r = fromFixed(b.radius), z = fromFixed(b.z);
		if (std::fabs(_walker.getX() - fromFixed(b.x)) <= r && std::fabs(_walker.getY() - fromFixed(b.y)) <= r &&
		    z <= head && z + fromFixed(b.boxHeight) >= feet + Walker::kStepHeight)
			inside = (int)i;
	}
	if (inside != _billboard) {
		if (_billboard >= 0)
			send((uint16)(billboards[_billboard].objectId | kTypeBillboard), kEventUntouch);
		_billboard = inside;
		if (inside >= 0)
			send((uint16)(billboards[inside].objectId | kTypeBillboard), kEventTouch);
	}
}

double Mission::pop32(ScriptVM &vm) {
	uint16 high = (uint16)vm.pop();
	uint16 low = (uint16)vm.pop();
	return fromFixed((int32)((uint32)high << 16 | low));
}

uint32 Mission::popRaw32(ScriptVM &vm) {
	uint16 high = (uint16)vm.pop();
	uint16 low = (uint16)vm.pop();
	return (uint32)high << 16 | low;
}

void Mission::push32(ScriptVM &vm, double value) {
	uint32 v = (uint32)toFixed(value);
	vm.push((int16)(v & 0xFFFF));
	vm.push((int16)(v >> 16));
}

Mission::Position Mission::popPosition(ScriptVM &vm) {
	Position position;
	position.x = pop32(vm);
	position.y = pop32(vm);
	position.z = pop32(vm);
	position.heading = vm.pop();
	return position;
}

void Mission::pushPosition(ScriptVM &vm, const Position &position) {
	vm.push(position.heading);
	push32(vm, position.z);
	push32(vm, position.y);
	push32(vm, position.x);
}

bool Mission::getPosition(uint16 id, Position &position) const {
	if (id == 0) {
		position.x = _walker.getX();
		position.y = _walker.getY();
		position.z = _walker.getFeet();
		double degrees = std::fmod(90 - _currentYaw * 180 / kPi, 360.0);
		position.heading = (int16)std::lround(degrees < 0 ? degrees + 360 : degrees);
		return true;
	}
	uint16 heading;
	if (_actors && _actors->getPosition(id, position.x, position.y, position.z, heading)) {
		position.heading = (int16)(heading * 360 / 65536);
		return true;
	}
	for (const LevelFile::Billboard &b : _level.getBillboards()) {
		if (isObject(b.objectId, kTypeBillboard, id)) {
			position = { fromFixed(b.x), fromFixed(b.y), fromFixed(b.z), 0 };
			return true;
		}
	}
	return false;
}

void Mission::setPosition(uint16 id, const Position &position) {
	if (_actors && _actors->isActor(id))
		_actors->setPosition(id, position.x, position.y, position.z, (uint16)(position.heading * 65536 / 360));
	for (LevelFile::Billboard &b : _level.getBillboards()) {
		if (isObject(b.objectId, kTypeBillboard, id)) {
			b.x = toFixed(position.x);
			b.y = toFixed(position.y);
			b.z = toFixed(position.z);
		}
	}
}

void Mission::setVisible(uint16 id, bool visible) {
	for (LevelFile::Wall &wall : _level.getWalls()) {
		if (isObject(wall.objectId, kTypeWall, id))
			wall.flags = visible ? wall.flags | LevelFile::kFlagVisible : wall.flags & ~LevelFile::kFlagVisible;
	}
	for (LevelFile::Sector &flat : _level.getSectors()) {
		if (isObject(flat.objectId, kTypeFlat, id))
			flat.flags = visible ? flat.flags | LevelFile::kFlagVisible : flat.flags & ~LevelFile::kFlagVisible;
	}
	for (LevelFile::Billboard &b : _level.getBillboards()) {
		if (isObject(b.objectId, kTypeBillboard, id))
			b.hidden = !visible;
	}
	// Showing an actor makes it idle (0x456BBC); hiding takes it out (mode 8).
	if (_actors && _actors->isActor(id))
		_actors->setMode(id, visible ? Actors::kModeIdle : Actors::kModeHidden);
}

void Mission::setActorMode(ScriptVM &vm, int mode, bool popTarget) {
	uint16 id = (uint16)vm.pop();
	uint16 target = popTarget ? (uint16)vm.pop() : 0;
	if (_actors)
		_actors->setMode(id, mode, target);
}

void Mission::setTexture(uint16 id, uint16 texture, uint16 frames, uint16 speed) {
	for (LevelFile::Wall &wall : _level.getWalls()) {
		if (isObject(wall.objectId, kTypeWall, id)) {
			wall.texture = texture;
			wall.frames = frames;
			wall.speed = speed;
		}
	}
	for (LevelFile::Sector &flat : _level.getSectors()) {
		if (isObject(flat.objectId, kTypeFlat, id)) {
			flat.texture = texture;
			flat.frames = frames;
			flat.speed = speed;
		}
	}
	for (LevelFile::Billboard &b : _level.getBillboards()) {
		if (isObject(b.objectId, kTypeBillboard, id)) {
			b.texture = texture;
			b.frames = frames;
			b.speed = speed;
		}
	}
}

void Mission::restoreTexture(uint16 id) {
	std::vector<LevelFile::Wall> &walls = _level.getWalls();
	for (size_t i = 0; i < walls.size(); i++) {
		if (isObject(walls[i].objectId, kTypeWall, id))
			walls[i].texture = _wallTextures[i];
	}
	std::vector<LevelFile::Sector> &flats = _level.getSectors();
	for (size_t i = 0; i < flats.size(); i++) {
		if (isObject(flats[i].objectId, kTypeFlat, id))
			flats[i].texture = _flatTextures[i];
	}
	std::vector<LevelFile::Billboard> &billboards = _level.getBillboards();
	for (size_t i = 0; i < billboards.size(); i++) {
		if (isObject(billboards[i].objectId, kTypeBillboard, id))
			billboards[i].texture = _billboardTextures[i];
	}
}

void Mission::setFullbright(uint16 id, bool on) {
	for (LevelFile::Wall &wall : _level.getWalls()) {
		if (isObject(wall.objectId, kTypeWall, id))
			wall.fullbright = on;
	}
	for (LevelFile::Sector &flat : _level.getSectors()) {
		if (isObject(flat.objectId, kTypeFlat, id))
			flat.fullbright = on;
	}
	for (LevelFile::Billboard &b : _level.getBillboards()) {
		if (isObject(b.objectId, kTypeBillboard, id))
			b.fullbright = on;
	}
}

void Mission::setPlayerHeading(int16 degrees) {
	// The original looks along (sin h, cos h) on the floor plan; the
	// renderer's yaw is measured from +x.
	_yaw = kPi / 2 - degrees * kPi / 180;
	_hasYaw = true;
}

void Mission::skipNative(ScriptVM &vm, uint16 op) {
	const OpcodeInfo *info = getOpcodeInfo(op);
	if (info) {
		for (int i = 0; i < info->pops; i++)
			vm.pop();
		for (int i = 0; i < info->pushes; i++)
			vm.push(0);
	}
	if (_missing[op]++ == 0)
		debugC(1, kDebugChannel, "native %02X (%s) is not implemented", op, info ? info->name : "?");
}

void Mission::unhandledEvent(ScriptVM &vm, ScriptInstance &self, uint16 event) {}

void Mission::callNative(ScriptVM &vm, ScriptInstance &self, uint16 op) {
	switch (op) {
	case OP_SEND_SIGNAL: {
		uint16 id = (uint16)vm.pop();
		int16 value = vm.pop();
		send(id, kEventSignal, { value });
		break;
	}
	case 0x2B: { // RANDOM
		int16 n = vm.pop();
		vm.push(n > 0 ? (int16)_random.getRandomNumber((uint32)n - 1) : 0);
		break;
	}
	case 0x2C: { // SEND_SET_VALUE
		uint16 id = (uint16)vm.pop();
		int16 value = vm.pop();
		send(id, kEventSetValue, { value });
		break;
	}
	case 0x2D: // RESET_DEFAULT
		restoreTexture((uint16)vm.pop());
		break;
	case 0x2E: // DISABLE_OBJECT
	case 0x2F: { // ENABLE_OBJECT
		int index = _vm->findInstance((uint16)vm.pop());
		if (index >= 0)
			_vm->getInstance(index).enabled = op == 0x2F;
		break;
	}
	case 0x30: { // UNSUBSCRIBE
		uint16 id = (uint16)vm.pop();
		_listeners.erase(std::remove(_listeners.begin(), _listeners.end(), id), _listeners.end());
		break;
	}
	case 0x31: { // SUBSCRIBE
		uint16 id = (uint16)vm.pop();
		if (std::find(_listeners.begin(), _listeners.end(), id) == _listeners.end())
			_listeners.insert(_listeners.begin(), id);
		break;
	}
	case 0x32: { // SET_TIMER_MS
		uint16 id = (uint16)vm.pop();
		setTimer(id, (uint16)vm.pop());
		break;
	}
	case 0x33: { // SET_TIMER_SECONDS
		uint16 id = (uint16)vm.pop();
		setTimer(id, (uint32)(uint16)vm.pop() * 1000);
		break;
	}
	case 0x34: { // SET_TIMER_RANDOM
		uint16 id = (uint16)vm.pop();
		int min = vm.pop(), max = vm.pop();
		int span = std::max(1, max - min + 1);
		setTimer(id, (uint32)std::max(0, min + (int)_random.getRandomNumber((uint32)span - 1)) * 100);
		break;
	}
	case 0x35: // KILL_TIMER
		_timers.erase((uint16)vm.pop());
		break;
	case 0x36: { // SET_SURFACE_TEXTURE
		uint16 id = (uint16)vm.pop();
		uint16 texture = (uint16)vm.pop();
		uint16 frames = (uint16)vm.pop();
		int16 period = vm.pop();
		setTexture(id, texture, frames, period > 0 ? (uint16)(0x4000 / period) : 0);
		break;
	}
	case 0x46: // HIDE
		setVisible((uint16)vm.pop(), false);
		break;
	case 0x47: // SHOW
		setVisible((uint16)vm.pop(), true);
		break;
	case 0x43: { // SET_PROXIMITY_RANGES
		int actorClass = vm.pop();
		int targetClass = vm.pop();
		double radii[5];
		for (int i = 0; i < 5; i++)
			radii[i] = pop32(vm);
		if (_actors)
			_actors->setProximity(actorClass, targetClass, radii);
		break;
	}
	case 0x44: { // SET_ACTOR_STEP_HEIGHT
		uint16 id = (uint16)vm.pop();
		double height = pop32(vm);
		if (_actors)
			_actors->setStepHeight(id, height);
		break;
	}
	case 0x48: // ACTOR_WANDER
		setActorMode(vm, Actors::kModeWander, false);
		break;
	case 0x49: // ACTOR_APPROACH_PLAYER
		setActorMode(vm, Actors::kModeWalkTo, false);
		break;
	case 0x4E: { // SET_ACTOR_COMBAT_PARAMS: the wander settings
		uint16 id = (uint16)vm.pop();
		bool followPlayer = vm.pop() != 0;
		vm.pop(); // guard target, not implemented
		uint16 minTurn = (uint16)vm.pop();
		uint16 maxTurn = (uint16)vm.pop();
		double inner = pop32(vm), home = pop32(vm), outer = pop32(vm);
		double minWalk = pop32(vm), maxWalk = pop32(vm);
		if (_actors)
			_actors->setWander(id, followPlayer, minTurn, maxTurn, inner, home, outer, minWalk, maxWalk);
		break;
	}
	case 0x4F: { // SET_ACTOR_WIDTH
		uint16 id = (uint16)vm.pop();
		double width = pop32(vm);
		if (_actors)
			_actors->setRadius(id, width / 2);
		break;
	}
	case 0x50: { // SET_ACTOR_HEIGHT
		uint16 id = (uint16)vm.pop();
		double height = pop32(vm);
		if (_actors)
			_actors->setBodyHeight(id, height);
		break;
	}
	case 0x53: { // SET_ACTOR_ANIM_RATE
		int mode = vm.pop();
		int rate = vm.pop();
		if (_actors)
			_actors->setAnimationRate(self.object->id, mode, (byte)rate);
		break;
	}
	case 0x54: // ACTOR_STATE_1
		setActorMode(vm, Actors::kModePain, false);
		break;
	case 0x55: // ACTOR_ATTACK
		setActorMode(vm, Actors::kModeAttack, true);
		break;
	case 0x56: // ACTOR_FACE
		setActorMode(vm, Actors::kModeFace, true);
		break;
	case 0x57: // ACTOR_STATE_4
		setActorMode(vm, Actors::kModeDeathA, false);
		break;
	case 0x58: // ACTOR_STATE_5
		setActorMode(vm, Actors::kModeDeathB, false);
		break;
	case 0x5A: { // SET_ACTOR_REACH
		uint16 id = (uint16)vm.pop();
		double reach = pop32(vm);
		if (_actors)
			_actors->setReach(id, reach);
		break;
	}
	case 0x4D: { // SET_POSITION
		uint16 id = (uint16)vm.pop();
		setPosition(id, popPosition(vm));
		break;
	}
	case 0x59: { // SET_HEADING
		uint16 id = (uint16)vm.pop();
		int16 heading = vm.pop();
		if (_actors)
			_actors->setHeading(id, (uint16)(heading * 65536 / 360));
		break;
	}
	case 0x70: { // HIDE_ACTOR
		uint16 id = (uint16)vm.pop();
		vm.pop();
		setVisible(id, false);
		break;
	}
	case 0x71: { // DROP_AT
		uint16 id = (uint16)vm.pop();
		uint16 where = (uint16)vm.pop();
		Position position;
		if (getPosition(where, position))
			setPosition(id, position);
		setVisible(id, true);
		break;
	}
	case 0x72: // TRIGGER
		send((uint16)vm.pop(), kEventActivate);
		break;
	case 0x75: { // TELEPORT_PLAYER
		Position position = popPosition(vm);
		debugC(1, kDebugChannel, "player to (%.1f, %.1f, %.1f) heading %d", position.x, position.y, position.z,
		       position.heading);
		_walker.place(position.x, position.y, position.z);
		setPlayerHeading(position.heading);
		break;
	}
	case 0x87: { // DAMAGE_PLAYER
		int16 amount = vm.pop();
		int16 type = vm.pop();
		_health = (int16)std::max(1, _health - std::max<int>(amount, 0));
		broadcast(kEventPlayerDamaged, { (int16)self.object->id, amount, amount, type });
		break;
	}
	case 0x89: { // ADD_HEALTH
		int16 amount = vm.pop();
		_health = (int16)std::max(1, std::min<int>(_maxHealth, _health + amount));
		broadcast(kEventPlayerHealed, { _health });
		break;
	}
	case 0xA2: { // SET_STEP_HEADROOM
		_walker.setHeadroom(pop32(vm));
		_walker.setStepHeight(pop32(vm));
		break;
	}
	case 0xB1: { // SET_PARENT
		int index = _vm->findInstance((uint16)vm.pop());
		self.parent = index >= 0 ? &_vm->getInstance(index) : nullptr;
		break;
	}
	case 0xB3: // SET_EYE_HEIGHT
		_walker.setEyeHeight(pop32(vm));
		break;
	case 0xB7: // SET_SKY_ENABLED
		_renderer.setSkyEnabled(vm.pop() != 0);
		break;
	case 0xBA: { // SCREEN_FLASH_NOTIFY: only its notification is implemented
		uint16 notify = (uint16)vm.pop();
		for (int i = 0; i < 4; i++)
			vm.pop(); // red, green, blue, percent
		uint32 total = 0;
		for (int i = 0; i < 3; i++) {
			uint16 high = (uint16)vm.pop(), low = (uint16)vm.pop();
			total += (uint32)high << 16 | low;
		}
		_effects.insert({ _time + total, notify });
		_missing[op]++;
		break;
	}
	case 0x68: { // PLAY_SOUND
		int sound = vm.pop();
		int volume = vm.pop();
		vm.push((int16)_sound->playSound(sound, volume));
		break;
	}
	case 0x69: { // PLAY_SOUND_DELAYED
		int sound = vm.pop();
		vm.pop();
		uint32 delay = popRaw32(vm);
		vm.push((int16)_sound->playDelayed(sound, delay));
		break;
	}
	case 0x6A: // PRELOAD_SOUND: files are read when played
		vm.pop();
		break;
	case 0x6B: // STOP_SOUND: takes a 16-bit value, which the original only matches against voices
		_sound->stopOne((uint32)(uint16)vm.pop() << 16);
		break;
	case 0x8D: { // PLAY_SOUND_CUE
		uint16 id = (uint16)vm.pop();
		int cue = vm.pop();
		_sound->playCue(cue, id);
		break;
	}
	case 0xCF: // STOP_ONE_SOUND
		_sound->stopOne(popRaw32(vm));
		break;
	case 0xD0: { // SET_SOUND_VOLUME
		uint32 reference = popRaw32(vm);
		_sound->setVolume(reference, vm.pop());
		break;
	}
	case 0xD1: { // SET_SOUND_LOOP
		int sound = vm.pop();
		int32 repeats = (int32)popRaw32(vm);
		uint32 minGap = (uint16)vm.pop();
		uint32 maxGap = (uint16)vm.pop();
		_sound->setLoop(sound, repeats, minGap, maxGap);
		break;
	}
	case 0xD2: { // SET_SOUND_PRIORITY
		uint32 reference = (uint16)vm.pop();
		_sound->setPriority(reference, vm.pop());
		break;
	}
	case 0xD3: { // SET_SOUND_POSITION
		uint32 reference = popRaw32(vm);
		double x = pop32(vm), y = pop32(vm), z = pop32(vm);
		_sound->setPosition(reference, x, y, z);
		break;
	}
	case 0xDC: { // PLAY_SOUND_FADE_IN
		uint32 reference = popRaw32(vm);
		uint32 ms = (uint16)vm.pop();
		vm.push((int16)_sound->playFadeIn(reference, ms));
		break;
	}
	case 0xDD: { // FADE_OUT_SOUND
		uint32 reference = popRaw32(vm);
		vm.push(_sound->fadeOut(reference, (uint16)vm.pop()) ? 0 : -1);
		break;
	}
	case 0xDE: { // MUTE_SOUND
		uint32 reference = popRaw32(vm);
		_sound->setMuted(reference, vm.pop() != 0);
		break;
	}
	case 0xDF:
	case 0xE0: { // SET_SOUND_OBJECT
		uint32 reference = popRaw32(vm);
		_sound->setObject(reference, (uint16)vm.pop());
		break;
	}
	case 0xE1: { // SET_SOUND_MAX_DISTANCE
		uint32 reference = popRaw32(vm);
		_sound->setRadius(reference, pop32(vm));
		break;
	}
	case 0xBE: { // DISTANCE_TO_PLAYER
		Position position;
		int16 distance = 0;
		if (getPosition((uint16)vm.pop(), position)) {
			double dx = position.x - _walker.getX(), dy = position.y - _walker.getY();
			double dz = position.z - _walker.getFeet();
			distance = (int16)std::min<long>(32767, std::lround(std::sqrt(dx * dx + dy * dy + dz * dz)));
		}
		vm.push(distance);
		break;
	}
	case 0xC6: // GET_HEALTH
		vm.push(_health);
		break;
	case 0xD9: // SET_PLAYER_WIDTH
		_walker.setRadius(pop32(vm) / 2);
		break;
	case 0xDB: { // GET_POSITION
		Position position = { 0, 0, 0, 0 };
		getPosition((uint16)vm.pop(), position);
		pushPosition(vm, position);
		break;
	}
	case 0xEC: { // SET_FULLBRIGHT
		uint16 id = (uint16)vm.pop();
		setFullbright(id, vm.pop() != 0);
		break;
	}
	case 0xF2: { // GET_ARRAY
		int16 index = vm.pop();
		vm.push(index >= 0 && index < kArraySize ? _array[index] : 0);
		break;
	}
	case 0xF3: { // SET_ARRAY
		int16 index = vm.pop();
		int16 value = vm.pop();
		if (index >= 0 && index < kArraySize)
			_array[index] = value;
		break;
	}
	default:
		skipNative(vm, op);
		break;
	}
}

} // End of namespace STG
