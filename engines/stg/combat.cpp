/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/combat.h"

#include <algorithm>
#include <cmath>

#include "common/audio/audiostream.h"
#include "common/debug.h"
#include "common/image/bmp.h"

namespace STG {

namespace {

const char *const kArchive = "COMBAT.DAT";
/** The viewscreen: the console's hole (0x54F650). */
const int kViewX = 52, kViewY = 10, kViewW = 532, kViewH = 231;
/** Rotation at full deflection: 228 angle units a frame at about 15 frames a second. */
const double kMaxTurn = 75 * M_PI / 180;
/** The AI fires every 2037 ticks of 1/291 s. */
const uint32 kAiFireMillis = 7000;
const uint32 kEndingMillis = 15000;
const uint32 kLifeSupportMillis = 30000;
const int kStars = 1500;

/** PROCEED's place on each COMBATn.BMP (table 0x548618). */
const int kProceed[25][2] = {
	{ 535, 429 }, { 374, 436 }, { 372, 118 }, { 364, 432 }, { 532, 132 }, { 376, 438 }, { 516, 428 },
	{ 376, 434 }, { 527, 430 }, { 508, 421 }, { 375, 434 }, { 518, 430 }, { 380, 432 }, { 380, 432 },
	{ 370, 446 }, { 380, 432 }, { 380, 432 }, { 370, 446 }, { 370, 446 }, { 370, 446 }, { 370, 446 },
	{ 380, 432 }, { 380, 432 }, { 380, 432 }, { 373, 125 }
};

/** Which shield faces each octant, by the number of shields (0x54E278). */
const int kFacing[5][8] = {
	{ 0, 0, 0, 0, 0, 0, 0, 0 }, { 4, 4, 4, 4, 4, 4, 4, 4 }, { 3, 3, 4, 4, 3, 3, 4, 4 },
	{ 2, 2, 3, 3, 2, 2, 4, 4 }, { 1, 1, 2, 2, 3, 3, 4, 4 }
};
/** The chance of blowing up for 4..8 systems destroyed (0x55009C). */
const int kExplodeChance[9] = { 0, 0, 0, 0, 10, 20, 40, 80, 85 };

/** The speech table's WAVs (0x54A980), by id. */
Common::String speechFile(int id) {
	if (id >= 0x6C && id <= 0x70)
		return Common::format("EXPLO%d.WAV", id - 0x6B);
	if (id >= 0x83 && id <= 0x85)
		return Common::format("CRUS%04d.WAV", 51 + id - 0x83);
	if (id == 0x86)
		return "DATA0103.WAV";
	if (id >= 0x87 && id <= 0x95)
		return Common::format("DATA%04d.WAV", 139 + id - 0x87);
	if (id >= 0x96 && id <= 0xAC)
		return Common::format("GEOR%04d.WAV", 143 + id - 0x96);
	if (id >= 0xAD && id <= 0xB7)
		return Common::format("PICA%04d.WAV", 262 + id - 0xAD);
	if (id >= 0xB9 && id <= 0xCA)
		return Common::format("RIKE%04d.WAV", 249 + id - 0xB9);
	if (id == 0xCB || id == 0xCC)
		return Common::format("TROI%04d.WAV", 76 + id - 0xCB);
	if (id >= 0xCD && id <= 0x10F)
		return Common::format("WORF%04d.WAV", 154 + id - 0xCD);
	if (id == 0x110)
		return "WORF0284.WAV";
	if (id == 0x111)
		return "AFUW0579.WAV";
	if (id == 0x112)
		return "AFUW0591.WAV";
	if (id >= 0x113 && id <= 0x130)
		return Common::format("COMP%04d.WAV", 34 + id - 0x113);
	return Common::String();
}

double clamp1(double v) {
	return std::max(-1.0, std::min(1.0, v));
}

uint32 darken(uint32 c, double f) {
	uint32 r = (uint32)(((c >> 16) & 0xFF) * f), g = (uint32)(((c >> 8) & 0xFF) * f), b = (uint32)((c & 0xFF) * f);
	return 0xFF000000u | r << 16 | g << 8 | b;
}

} // End of anonymous namespace

struct SpaceCombat::Ship {
	Common::String type;
	/** The class's three letters, also the console pictures' (SENT_0...). */
	Common::String token;
	/** The effects' prefix: E, K, R, C, P or _. */
	char effects = '_';
	const ShipClass *cls = nullptr;
	const Model3D *model = nullptr, *lowModel = nullptr;
	const std::vector<CombatMaterial> *materials = nullptr, *lowMaterials = nullptr;
	double radius = 100;
	Vec3 position;
	Mat3 orientation;
	double speed = 0, setting = 0;
	double yawIn = 0, pitchIn = 0, rollIn = 0;
	double energy[ShipClass::kSystemCount] = {};
	double damage[ShipClass::kSystemCount] = {};
	double reload[4] = {};
	int loaded[4] = {};
	int stock = 0;
	bool alive = true, hidden = false, gone = false;
	int wave = 1;
	/** 1 Soran's own ship, 2 the Q'urash. */
	int soran = 0;
	int aimed = -1;
	int number = 1;
	uint32 aiTimer = 3000;
	int aiState = 0;
	uint32 aiStateTime = 0;
	Vec3 waypoint;
	int destroyedReported = 0;
};

SpaceCombat::SpaceCombat(const ResourceManager &resources, Audio::Mixer *mixer, Common::RandomSource &random)
	: _resources(resources), _mixer(mixer), _random(random) {
	_renderer.setViewport(kViewW, kViewH, 60);
}

SpaceCombat::~SpaceCombat() {
	silence();
}

void SpaceCombat::silence() {
	if (!_mixer)
		return;
	_mixer->stopHandle(_speech);
	_mixer->stopHandle(_music);
	_mixer->stopHandle(_effect);
}

// Data

std::unique_ptr<Common::SeekableReadStream> SpaceCombat::open(const Common::String &name) const {
	return _resources.open({ kArchive, name });
}

const CombatImage *SpaceCombat::image(const Common::String &name) {
	Common::String key = Common::toUpper(name);
	auto it = _images.find(key);
	if (it != _images.end())
		return it->second.get();
	std::unique_ptr<CombatImage> result(new CombatImage());
	std::unique_ptr<Common::SeekableReadStream> stream = open(key);
	if (!stream || !result->loadRas(*stream))
		result.reset();
	const CombatImage *found = result.get();
	_images[key] = std::move(result);
	return found;
}

const CombatImage *SpaceCombat::texture(const Common::String &name) {
	Common::String key = "tex:" + Common::toUpper(name);
	auto it = _images.find(key);
	if (it != _images.end())
		return it->second.get();
	std::unique_ptr<CombatImage> result(new CombatImage());
	std::unique_ptr<Common::SeekableReadStream> stream = open(name);
	bool ok = stream && (Common::hasSuffixIgnoreCase(name, ".lbm") ? result->loadLbm(*stream) : result->loadGif(*stream));
	if (!ok)
		result.reset();
	const CombatImage *found = result.get();
	_images[key] = std::move(result);
	return found;
}

const SprAnimation *SpaceCombat::sprite(const Common::String &name) {
	Common::String key = Common::toUpper(name);
	auto it = _sprites.find(key);
	if (it != _sprites.end())
		return it->second.get();
	std::unique_ptr<SprAnimation> result(new SprAnimation());
	std::unique_ptr<Common::SeekableReadStream> stream = open(key);
	if (!stream || !result->load(*stream))
		result.reset();
	const SprAnimation *found = result.get();
	_sprites[key] = std::move(result);
	return found;
}

const Model3D *SpaceCombat::model(const Common::String &name) {
	Common::String key = Common::toUpper(name);
	auto it = _models.find(key);
	if (it != _models.end())
		return it->second.get();
	std::unique_ptr<Model3D> result(new Model3D());
	std::unique_ptr<Common::SeekableReadStream> stream = open(key);
	if (!stream || !result->load(*stream))
		result.reset();
	const Model3D *found = result.get();
	_models[key] = std::move(result);
	return found;
}

const std::vector<CombatMaterial> *SpaceCombat::materials(const Common::String &name) {
	Common::String key = Common::toUpper(name);
	auto it = _materials.find(key);
	if (it != _materials.end())
		return it->second.get();
	std::unique_ptr<std::vector<CombatMaterial>> result(new std::vector<CombatMaterial>());
	std::unique_ptr<Common::SeekableReadStream> stream = open(key);
	if (!stream || !CombatMaterial::loadList(*stream, *result))
		result.reset();
	const std::vector<CombatMaterial> *found = result.get();
	_materials[key] = std::move(result);
	return found;
}

const ShipClass *SpaceCombat::findClass(const Common::String &token) const {
	for (const ShipClass &c : _classes) {
		if (c.token == token)
			return &c;
	}
	return _classes.empty() ? nullptr : &_classes[0];
}

bool SpaceCombat::load(int mission) {
	_mission = mission;
	std::unique_ptr<Common::SeekableReadStream> systems = open("SYSTEMS.DAT");
	if (!systems)
		return false;
	std::vector<byte> text = systems->readAll();
	if (!ShipClass::loadAll(Common::String(text.begin(), text.end()), _classes))
		return false;
	std::unique_ptr<Common::SeekableReadStream> scenario = open(Common::format("SC%d.MIS", mission));
	if (!scenario)
		return false;
	text = scenario->readAll();
	std::vector<MissionShip> lines;
	if (!loadCombatMission(Common::String(text.begin(), text.end()), lines))
		return false;
	std::unique_ptr<Common::SeekableReadStream> pal = open("STANDARD.PAL");
	if (pal)
		loadStandardPalette(*pal, _palette);
	std::unique_ptr<Common::SeekableReadStream> font = open("FONTSCAN.MET");
	if (font)
		_font.load(*font);

	// The Enterprise first.
	_ships.clear();
	for (const MissionShip &line : lines) {
		if (line.type == "player")
			addShip(line);
	}
	if (_ships.empty())
		return false;
	for (const MissionShip &line : lines) {
		if (line.type != "player")
			addShip(line);
	}

	// The briefing.
	Image::BitmapDecoder decoder;
	std::unique_ptr<Common::SeekableReadStream> brief = open(Common::format("COMBAT%d.BMP", mission));
	if (brief && decoder.loadStream(*brief) && decoder.hasPalette())
		_briefing.load(decoder);
	Image::BitmapDecoder proceed;
	std::unique_ptr<Common::SeekableReadStream> button = open("PROCEED.BMP");
	if (button && proceed.loadStream(*button) && proceed.hasPalette())
		_proceed.load(proceed, 2);
	if (mission >= 1 && mission <= 25) {
		_proceedX = kProceed[mission - 1][0];
		_proceedY = kProceed[mission - 1][1];
	}
	_phase = _briefing.isLoaded() ? kBriefing : kFight;
	if (_phase == kBriefing) {
		ResourceRef ref = { "OPTIONS.DAT", Common::format("CLOOP%d.WAV", 1 + (int)_random.getRandomNumber(3)) };
		std::unique_ptr<Audio::RewindableAudioStream> stream = _resources.loadSound(ref);
		if (stream && _mixer)
			_mixer->playStream(Audio::Mixer::kMusicSoundType, &_music,
			                   Audio::makeLoopingAudioStream(std::move(stream), 0));
	} else {
		playMusic(Common::format("SCLOOP%d.WAV", 1 + (int)_random.getRandomNumber(3)), true);
	}

	for (int i = 0; i < kStars; i++) {
		Vec3 star((int)_random.getRandomNumber(2000) - 1000, (int)_random.getRandomNumber(2000) - 1000,
		          (int)_random.getRandomNumber(2000) - 1000);
		_stars.push_back(star.normalized());
	}
	nextTarget(false);
	return true;
}

void SpaceCombat::addShip(const MissionShip &line) {
	// 0x4A4DA2: the types, their models and classes.
	struct Type {
		const char *name, *model, *token;
		char effects;
	};
	static const Type kTypes[] = {
		{ "player", "ENTERPRS", "ent", 'E' }, { "sm_klingon", "KLINGON", "bop", 'K' },
		{ "klingon", "VORCHA", "vor", 'K' },  { "romulan", "ROMULAN", "war", 'R' },
		{ "chodak", "CHODAK", "cho", 'C' },   { "garidian", "GARIDIAN", "war", 'R' },
		{ "probe", "PROBE", "pro", 'P' },     { "starbase", "STARBASE", "pro", '_' }
	};
	const Type *type = nullptr;
	for (const Type &t : kTypes) {
		if (line.type == t.name)
			type = &t;
	}
	if (!type) {
		warning("Space combat: no ship type %s", line.type.c_str());
		return;
	}
	Ship ship;
	ship.type = line.type;
	ship.token = type->token;
	ship.effects = type->effects;
	ship.cls = findClass(type->token);
	ship.model = model(Common::String(type->model) + ".3DV");
	ship.materials = materials(Common::String(type->model) + ".MTL");
	ship.lowModel = model(Common::String(type->model) + ".3DR");
	ship.lowMaterials = materials(Common::String(type->model) + ".MTR");
	if (ship.model)
		ship.radius = std::max(50.0f, ship.model->radius);
	// The mission's (x, y, z) is east, north and altitude (0x4A36A4).
	ship.position = Vec3(line.x, line.z, line.y);
	ship.orientation = Mat3::fromYawPitch(line.yaw * M_PI / 180, line.pitch * M_PI / 180);
	ship.wave = line.wave;
	ship.hidden = line.wave > 1;
	ship.soran = line.ai == "soran" || line.ai == "soran9" ? 1 : line.ai == "quarash" ? 2 : 0;
	if (line.ai.compare(0, 9, "standard-") == 0) {
		int system = std::atoi(line.ai.c_str() + 9);
		ship.aimed = system >= 5 && system <= 12 ? system : -1;
	}
	if (ship.cls) {
		for (int s = 0; s < ShipClass::kSystemCount; s++)
			ship.energy[s] = std::max(0, ship.cls->systems[s].max);
		for (int t = 0; t < 4; t++)
			ship.loaded[t] = std::max(0, ship.cls->systems[ShipClass::kPhoton0 + t].max);
		ship.stock = ship.cls->torpedoes;
	}
	ship.aiTimer = 2000 + _random.getRandomNumber(5000);
	for (const Ship &other : _ships) {
		if (other.type == ship.type)
			ship.number++;
	}
	_ships.push_back(ship);
}

double SpaceCombat::systemFraction(const Ship &ship, int system) const {
	if (!ship.cls || ship.cls->systems[system].max <= 0)
		return 0;
	double max = ship.cls->systems[system].max;
	return std::max(0.0, (max - ship.damage[system]) / max);
}

bool SpaceCombat::isEnemyAlive(const Ship &ship) const {
	return ship.alive && !ship.hidden && !ship.gone;
}

// Simulation

void SpaceCombat::update(uint32 ms) {
	ms = std::min<uint32>(ms, 100);
	_time += ms;
	if (_subtitleLeft)
		_subtitleLeft = ms >= _subtitleLeft ? 0 : _subtitleLeft - ms;
	if (_mixer && !_mixer->isSoundHandleActive(_speech) && !_subtitleLeft)
		_subtitle.clear();
	if (_messageLeft)
		_messageLeft = ms >= _messageLeft ? 0 : _messageLeft - ms;

	if (_phase == kBriefing)
		return;
	double dt = ms / 1000.0;
	if (_phase == kEnding) {
		_endTime += ms;
		bool quiet = !_mixer || (!_mixer->isSoundHandleActive(_speech) && !_mixer->isSoundHandleActive(_music));
		if (_endTime >= kEndingMillis || (quiet && _endTime >= 3000))
			_finished = true;
	}

	_phaserLockout = _phaserLockout > ms ? _phaserLockout - ms : 0;
	_photonLockout = _photonLockout > ms ? _photonLockout - ms : 0;
	if (_phase == kFight)
		updateHelm(dt);
	for (int i = 0; i < (int)_ships.size(); i++) {
		Ship &ship = _ships[i];
		if (!ship.alive || ship.hidden || ship.gone)
			continue;
		if (i > 0 && _phase == kFight)
			updateAi(ship, dt);
		updateShip(ship, dt);
	}
	updateWeapons(ms);
	for (Explosion &e : _explosions)
		e.time += ms;
	_explosions.erase(std::remove_if(_explosions.begin(), _explosions.end(),
	                                 [](const Explosion &e) {
		                                 return !e.sprite || e.time >= e.sprite->frames.size() * e.sprite->frameMillis;
	                                 }),
	                  _explosions.end());
	if (_phase == kFight)
		checkEnd();
}

void SpaceCombat::updateShip(Ship &ship, double dt) {
	const ShipClass *cls = ship.cls;
	if (!cls)
		return;
	// Shields and phaser banks recharge; the tubes reload from the stock.
	double power = std::max(0.25, systemFraction(ship, ShipClass::kPower));
	for (int s = ShipClass::kShield1; s <= ShipClass::kShield1 + 3; s++) {
		double cap = std::max(0.0, cls->systems[s].max - ship.damage[s]);
		ship.energy[s] = std::min(cap, ship.energy[s] + cls->systems[s].charge * dt * power * 0.3);
	}
	double weapons = systemFraction(ship, ShipClass::kWeapons);
	for (int s = ShipClass::kPhaser0; s < ShipClass::kPhoton0; s++) {
		double cap = std::max(0.0, cls->systems[s].max - ship.damage[s]);
		ship.energy[s] = std::min(cap, ship.energy[s] + cls->systems[s].charge * dt * weapons * power * 0.15);
	}
	double life = systemFraction(ship, ShipClass::kLifeSupport);
	for (int t = 0; t < 4; t++) {
		int max = cls->systems[ShipClass::kPhoton0 + t].max;
		if (max <= 0 || ship.loaded[t] >= max || ship.stock <= 0)
			continue;
		// 8 - 7 x life support (0x4A97B7): slower with the crew in trouble.
		ship.reload[t] += dt;
		if (ship.reload[t] >= 1.5 + 6.5 * (1 - life)) {
			ship.reload[t] = 0;
			ship.loaded[t]++;
			ship.stock--;
		}
	}
	// Damage control mends what isn't destroyed.
	double repair = systemFraction(ship, ShipClass::kDamageControl);
	for (int s = 1; s < ShipClass::kSystemCount; s++) {
		int max = cls->systems[s].max;
		if (max > 0 && ship.damage[s] > 0 && ship.damage[s] < max)
			ship.damage[s] = std::max(0.0, ship.damage[s] - cls->systems[s].repair * dt * repair * 0.01);
	}
	fly(ship, dt);
}

void SpaceCombat::fly(Ship &ship, double dt) {
	// Turning slows with speed (table 0x54F150: 1.0 at rest to 0.21 flat out).
	double maxSpeed = ship.cls ? ship.cls->maxSpeed : 500;
	double fraction = std::min(1.0, std::fabs(ship.speed) / std::max(1.0, maxSpeed));
	double rate = kMaxTurn * std::min(1.0, (ship.cls ? ship.cls->agility : 50) / 100.0) * (1 - 0.79 * fraction);
	ship.orientation.rotate(ship.yawIn * rate * dt, ship.pitchIn * rate * dt, ship.rollIn * rate * dt);
	double engines = systemFraction(ship, ShipClass::kEngines);
	double target = ship.setting / 100 * maxSpeed * engines;
	ship.speed += (target - ship.speed) * std::min(1.0, dt * 1.2);
	ship.position = ship.position + ship.orientation.forward * (ship.speed * dt);
}

void SpaceCombat::steer(Ship &ship, const Vec3 &point, double dt, bool away) {
	Vec3 local = ship.orientation.inverse(point - ship.position);
	if (away)
		local = local * -1;
	double yaw = std::atan2(local.x, local.z), pitch = std::atan2(local.y, std::hypot(local.x, local.z));
	ship.yawIn = clamp1(yaw * 2);
	ship.pitchIn = clamp1(pitch * 2);
	// Keep the wings level.
	ship.rollIn = clamp1(ship.orientation.right.y * 2);
	(void)dt;
}

void SpaceCombat::updateAi(Ship &ship, double dt) {
	const Ship &enterprise = _ships[0];
	if (!enterprise.alive || !ship.cls)
		return;
	Vec3 toTarget = enterprise.position - ship.position;
	double distance = toTarget.length();
	uint32 ms = (uint32)(dt * 1000);
	ship.aiStateTime += ms;

	int destroyed = 0;
	for (int s = ShipClass::kWeapons; s <= ShipClass::kCloak; s++)
		destroyed += ship.cls->systems[s].max > 0 && ship.damage[s] >= ship.cls->systems[s].max;
	bool others = false;
	for (const Ship &s : _ships)
		others |= &s != &ship && &s != &_ships[0] && (isEnemyAlive(s) || (s.hidden && s.alive));

	if (destroyed >= 2 && !ship.soran) {
		// Badly hurt: run (0x4AFF2A), and once far enough, gone.
		steer(ship, enterprise.position, dt, true);
		ship.setting = 100;
		if (distance > 2.5 * ship.cls->phaserRange)
			ship.gone = true;
	} else if (ship.soran && others) {
		// Soran keeps away until his escort is gone.
		double keep = 2.0 * ship.cls->phaserRange;
		steer(ship, enterprise.position, dt, distance < keep);
		ship.setting = distance < keep ? 100 : 20;
	} else if (distance < (ship.radius + enterprise.radius) * 1.5) {
		steer(ship, enterprise.position, dt, true);
		ship.setting = 100;
	} else if (ship.aiState == 0) {
		// An attack run, then a break and a loop back.
		steer(ship, enterprise.position + enterprise.orientation.forward * (enterprise.speed * 1.0), dt);
		ship.setting = distance > ship.cls->closeRadius ? 100 : 60;
		if (distance < std::max(1500.0, ship.cls->closeRadius * 0.5)) {
			ship.aiState = 1;
			ship.aiStateTime = 0;
			Vec3 side = ship.orientation.right * ((_random.getRandomNumber(1) ? 1 : -1) * 6000.0);
			ship.waypoint = ship.position + ship.orientation.forward * 8000.0 + side + Vec3(0, (int)_random.getRandomNumber(4000) - 2000.0, 0);
		}
	} else {
		steer(ship, ship.waypoint, dt);
		ship.setting = 100;
		if (ship.aiStateTime > 5000)
			ship.aiState = 0;
	}

	// Fire every seven seconds or so, in range.
	ship.aiTimer = ship.aiTimer > ms ? ship.aiTimer - ms : 0;
	if (ship.aiTimer == 0) {
		ship.aiTimer = kAiFireMillis - 1000 + _random.getRandomNumber(2000);
		int index = (int)(&ship - &_ships[0]);
		if (distance <= ship.cls->phaserRange)
			firePhasers(index);
		if (ship.cls->photons > 0 && distance <= ship.cls->photonRange && _random.getRandomNumber(1) == 0)
			firePhotons(index);
	}
}

void SpaceCombat::updateHelm(double dt) {
	Ship &e = _ships[0];
	if (!e.alive)
		return;
	const Ship *target = _target >= 0 && _target < (int)_ships.size() && isEnemyAlive(_ships[_target]) ? &_ships[_target] : nullptr;
	switch (_helm) {
	case kStandBy:
	case kMaintainStatus: {
		// By hand: the inputs follow the keys, and fall back when let go.
		double *inputs[3] = { &e.yawIn, &e.pitchIn, &e.rollIn };
		for (int i = 0; i < 3; i++) {
			double want = _turnKeys[i];
			double step = dt * (want != 0 ? 2.0 : 3.0);
			*inputs[i] += clamp1((want - *inputs[i]) / std::max(step, 1e-6)) * step;
			*inputs[i] = clamp1(*inputs[i]);
		}
		if (_speedKey)
			e.setting = std::max(-100.0, std::min(100.0, e.setting + _speedKey * 60 * dt));
		break;
	}
	case kClose:
		if (target) {
			steer(e, target->position, dt);
			double d = (target->position - e.position).length();
			e.setting = d > e.cls->closeRadius ? 100 : 30;
		}
		break;
	case kEvade: {
		Vec3 centre;
		int n = 0;
		for (size_t i = 1; i < _ships.size(); i++) {
			if (isEnemyAlive(_ships[i])) {
				centre = centre + _ships[i].position;
				n++;
			}
		}
		if (n)
			steer(e, centre * (1.0 / n), dt, true);
		e.setting = 100;
		break;
	}
	case kMaintain:
		if (target) {
			steer(e, target->position, dt);
			double d = (target->position - e.position).length();
			e.setting = std::max(-50.0, std::min(100.0, (d - _keepRange) / 50));
		}
		break;
	case kFullStop:
		e.yawIn = e.pitchIn = e.rollIn = 0;
		e.setting = 0;
		break;
	case kWarpOut:
		break;
	}
}

int SpaceCombat::octant(const Ship &ship, const Vec3 &point) const {
	Vec3 local = ship.orientation.inverse(point - ship.position);
	return (local.x > 0 ? 4 : 0) | (local.z < 0 ? 2 : 0) | (local.y > 0 ? 1 : 0);
}

int SpaceCombat::facing(const Ship &target, const Vec3 &from) const {
	int shields = target.cls ? std::max(0, std::min(4, target.cls->shields)) : 0;
	return kFacing[shields][octant(target, from)];
}

void SpaceCombat::firePhasers(int shooter) {
	Ship &ship = _ships[shooter];
	int targetIndex = shooter == 0 ? _target : 0;
	if (!ship.cls || targetIndex < 0 || targetIndex >= (int)_ships.size() || !isEnemyAlive(_ships[targetIndex]) ||
	    !_ships[targetIndex].alive)
		return;
	const Ship &target = _ships[targetIndex];
	double distance = (target.position - ship.position).length();
	if (distance > ship.cls->phaserRange) {
		if (shooter == 0)
			speak(0x112);
		return;
	}
	int arc = octant(ship, target.position);
	double computer = std::max(0.5, systemFraction(ship, ShipClass::kComputer));
	bool fired = false;
	for (int s = ShipClass::kPhaser0; s < ShipClass::kPhoton0; s++) {
		const ShipClass::SystemInfo &bank = ship.cls->systems[s];
		if (bank.max <= 0 || !(bank.arcs & (1 << arc)) || ship.energy[s] < 1)
			continue;
		// The hit roll (0x49BE85).
		double range = ship.cls->phaserRange;
		double score = computer * ((range - distance + target.radius) * 0.75 / range + 0.25);
		bool hit = score >= 0.11 * (1 + _random.getRandomNumber(1000) / 1000.0);
		Beam beam;
		beam.from = shooter;
		beam.to = targetIndex;
		beam.end = target.position;
		if (!hit) {
			Vec3 off((int)_random.getRandomNumber(2000) - 1000, (int)_random.getRandomNumber(2000) - 1000,
			         (int)_random.getRandomNumber(2000) - 1000);
			beam.end = target.position + off.normalized() * (distance * 0.3 + target.radius * 2);
		}
		beam.time = 0;
		beam.colour = ship.effects == 'E' ? 0xFFFF7020u : ship.effects == 'R' ? 0xFF40FF60u : ship.effects == 'C' ? 0xFF60A0FFu : 0xFFFF3030u;
		beam.damage = (int)ship.energy[s];
		beam.hit = hit;
		beam.applied = false;
		beam.facingFrom = 0;
		ship.energy[s] = 0;
		_beams.push_back(beam);
		fired = true;
	}
	if (fired) {
		playEffect(Common::format("%c_PHASER.WAV", ship.effects == 'P' ? '_' : ship.effects),
		           (int)std::max(40.0, 255 * (1 - std::min(1.0, (ship.position - _ships[0].position).length() / 8500))));
	}
}

void SpaceCombat::firePhotons(int shooter) {
	Ship &ship = _ships[shooter];
	int targetIndex = shooter == 0 ? _target : 0;
	if (!ship.cls || targetIndex < 0 || targetIndex >= (int)_ships.size() || !isEnemyAlive(_ships[targetIndex]))
		return;
	const Ship &target = _ships[targetIndex];
	int arc = octant(ship, target.position);
	// The tube facing the target fires all it holds (0x4999AB).
	for (int t = 0; t < 4; t++) {
		int s = ShipClass::kPhoton0 + t;
		const ShipClass::SystemInfo &tube = ship.cls->systems[s];
		if (tube.max <= 0 || !(tube.arcs & (1 << arc)) || ship.damage[s] >= tube.max)
			continue;
		if (ship.loaded[t] <= 0) {
			if (shooter == 0)
				speak(0x111);
			return;
		}
		int count = ship.loaded[t];
		ship.loaded[t] = 0;
		Vec3 aim = (target.position - ship.position).normalized();
		for (int i = 0; i < count; i++) {
			Torpedo torpedo;
			double spread = (i - (count - 1) / 2.0) * 0.08;
			Vec3 dir = (aim + ship.orientation.right * spread).normalized();
			torpedo.position = ship.position + dir * ship.radius;
			torpedo.velocity = dir * std::max(2000.0, std::fabs(ship.speed) + 2000);
			torpedo.owner = shooter;
			torpedo.target = targetIndex;
			torpedo.life = 20000;
			torpedo.damage = ship.cls->photonDamage;
			torpedo.sprite = sprite(Common::format("%c_PHOTON.SPR", ship.effects == 'P' ? '_' : ship.effects));
			if (!torpedo.sprite)
				torpedo.sprite = sprite("__PHOTON.SPR");
			_torpedoes.push_back(torpedo);
		}
		playEffect(Common::format("%c_PHOTON.WAV", ship.effects == 'P' ? '_' : ship.effects));
		return;
	}
}

void SpaceCombat::updateWeapons(uint32 ms) {
	// Beams hurt as their animation reaches its fourth frame.
	for (Beam &beam : _beams) {
		beam.time += ms;
		if (!beam.applied && beam.time >= 300) {
			beam.applied = true;
			if (beam.hit && beam.to < (int)_ships.size() && _ships[beam.to].alive && !_ships[beam.to].gone) {
				const Ship &from = _ships[beam.from];
				int aimed = beam.from == 0 ? _aimedSystem : from.aimed;
				hit(beam.to, facing(_ships[beam.to], from.position), beam.damage, aimed);
			}
		}
	}
	_beams.erase(std::remove_if(_beams.begin(), _beams.end(), [](const Beam &b) { return b.time >= 600; }),
	             _beams.end());

	double dt = ms / 1000.0;
	for (Torpedo &t : _torpedoes) {
		t.life = t.life > ms ? t.life - ms : 0;
		if (t.target >= 0 && t.target < (int)_ships.size() && _ships[t.target].alive && !_ships[t.target].gone) {
			const Ship &target = _ships[t.target];
			Vec3 to = target.position - t.position;
			double speed = std::max(1750.0, t.velocity.length());
			// Homing, with a limited turn.
			Vec3 want = to.normalized() * speed;
			t.velocity = (t.velocity + (want - t.velocity) * std::min(1.0, dt * 2.5)).normalized() * speed;
			if (to.length() < target.radius + speed * dt) {
				int owner = t.owner;
				hit(t.target, facing(target, t.position), t.damage, owner == 0 ? _aimedSystem : _ships[owner].aimed);
				Explosion e;
				e.position = t.position;
				e.size = 400;
				e.time = 0;
				e.sprite = sprite(Common::format("%c_SHIELD.SPR", target.effects == 'P' ? '_' : target.effects));
				if (e.sprite)
					_explosions.push_back(e);
				t.life = 0;
				continue;
			}
		}
		t.position = t.position + t.velocity * dt;
	}
	_torpedoes.erase(std::remove_if(_torpedoes.begin(), _torpedoes.end(), [](const Torpedo &t) { return t.life == 0; }),
	                 _torpedoes.end());
}

void SpaceCombat::hit(int index, int shield, double amount, int aimed) {
	Ship &ship = _ships[index];
	if (!ship.alive || !ship.cls || _phase != kFight)
		return;
	const ShipClass &cls = *ship.cls;
	// The difficulty weighs on the Enterprise (0x4B08C3).
	if (index == 0)
		amount *= _difficulty == 0 ? 1.2 : _difficulty == 2 ? 0.85 : 1.0;
	if (ship.token == "cho")
		amount /= 2;

	// The shield takes it, or some of it leaks in (0x4B0103).
	double leak = amount;
	if (shield >= 1 && shield <= 4 && cls.systems[shield].max > 0 && ship.damage[shield] < cls.systems[shield].max) {
		double energy = ship.energy[shield];
		double part;
		if (energy < amount) {
			part = energy;
			leak = amount - energy;
		} else {
			int column = std::max(0, std::min(12, (int)((120 - energy) / 10)));
			if ((int)_random.getRandomNumber(99) < cls.bleed[shield][column]) {
				leak = amount * 2 / 3;
				part = amount / 3;
			} else {
				leak = 0;
				part = amount;
			}
		}
		ship.energy[shield] = std::max(0.0, ship.energy[shield] - part);
		bool wasUp = ship.damage[shield] < cls.systems[shield].max * 0.5;
		ship.damage[shield] += part * 0.5;
		bool down = ship.damage[shield] >= cls.systems[shield].max;
		if (down)
			ship.energy[shield] = 0;
		if (index == 0) {
			static const int kHalf[5] = { 0, 0xBA, 0xBE, 0xC0, 0xBC }, kGone[5] = { 0, 0xBB, 0xBF, 0xC1, 0xBD };
			if (down)
				speak(kGone[shield]);
			else if (wasUp && ship.damage[shield] >= cls.systems[shield].max * 0.5)
				speak(kHalf[shield]);
			playEffect(Common::format("SHIELDS%d.WAV", 1 + (int)_random.getRandomNumber(2)));
		} else if (down) {
			static const int kTheirs[5] = { 0, 0xFD, 0x100, 0xFF, 0xFE };
			speak(kTheirs[shield]);
		}
	}

	// What leaks in hits a system: the one aimed at, or one at random.
	for (int step = 0; step < 5 && leak > 0.5; step++) {
		int system = -1;
		if (aimed >= 5 && aimed <= 12 && cls.systems[aimed].max > 0 && ship.damage[aimed] < cls.systems[aimed].max &&
		    (int)_random.getRandomNumber(99) <= cls.systems[aimed].aimed) {
			system = aimed;
		} else {
			int r = (int)_random.getRandomNumber(99);
			for (int s = 12; s >= 5; s--) {
				if (cls.systems[s].max > 0 && cls.systems[s].threshold <= r) {
					system = s;
					break;
				}
			}
		}
		if (system < 0)
			break;
		double room = cls.systems[system].max - ship.damage[system];
		if (room <= 0) {
			leak *= 0.8;
			continue;
		}
		double taken = std::min(room, leak);
		bool before = ship.damage[system] >= cls.systems[system].max;
		ship.damage[system] += taken;
		leak = (leak - taken) * 0.9;
		bool now = ship.damage[system] >= cls.systems[system].max;
		if (now && !before) {
			if (index == 0) {
				static const int kLines[13] = { 0, 0, 0, 0, 0, 0xD2, 0x84, 0x9F, 0xAB, 0x91, 0x8B, 0xC7, 0 };
				speak(kLines[system]);
				if (system == ShipClass::kLifeSupport)
					_lifeSupportOut = 1;
			} else {
				speak(0x104 + system - 5);
			}
		}
	}

	// Enough systems gone, and the ship blows up (0x55009C).
	int destroyed = 0;
	for (int s = ShipClass::kWeapons; s <= ShipClass::kCloak; s++)
		destroyed += cls.systems[s].max > 0 && ship.damage[s] >= cls.systems[s].max;
	if ((int)_random.getRandomNumber(99) < kExplodeChance[std::min(8, destroyed)]) {
		destroy(index);
		return;
	}
	// Soran's ship is beaten once it can't run or has no power.
	if (ship.soran && (ship.damage[ShipClass::kEngines] >= cls.systems[ShipClass::kEngines].max ||
	                   ship.damage[ShipClass::kPower] >= cls.systems[ShipClass::kPower].max)) {
		if (ship.soran == 2) {
			speak(0xB9, true);
			speak(0xB6, true);
		} else {
			speak(_random.getRandomNumber(1) ? 0xCD : 0xCE, true);
		}
		ship.gone = true;
		finish(kWon);
	}
}

void SpaceCombat::destroy(int index) {
	Ship &ship = _ships[index];
	ship.alive = false;
	Explosion e;
	e.position = ship.position;
	e.size = std::max(400.0, ship.radius * 4);
	e.time = 0;
	e.sprite = sprite(Common::format("%d_BOOM.SPR", 1 + (int)_random.getRandomNumber(4)));
	if (e.sprite)
		_explosions.push_back(e);
	playEffect(Common::format("EXPLO%d.WAV", 1 + (int)_random.getRandomNumber(3)));
	if (index == 0) {
		speak(0xC8, true);
		finish(kDestroyed);
		return;
	}
	speak(0x10D);
	if (index == _target)
		nextTarget(false);
}

void SpaceCombat::nextTarget(bool announce) {
	int count = (int)_ships.size();
	for (int k = 1; k <= count; k++) {
		int i = ((_target < 1 ? 0 : _target) + k) % count;
		if (i == 0 || !isEnemyAlive(_ships[i]))
			continue;
		_target = i;
		if (announce) {
			const Ship &t = _ships[i];
			int base = t.token == "war" ? 0xEA : t.token == "bop" ? 0xEF : t.token == "cho" ? 0xF5 : t.token == "vor" ? 0xFB : 0xFA;
			int soranLine = t.token == "war" ? 0xEE : t.token == "bop" ? 0xF4 : t.token == "cho" ? 0xF9 : 0x110;
			speak(t.soran ? soranLine : base + std::min(t.number - 1, t.token == "bop" ? 4 : 3));
		}
		return;
	}
	_target = -1;
}

void SpaceCombat::checkEnd() {
	Ship &e = _ships[0];
	if (_lifeSupportOut) {
		_lifeSupportOut += 16;
		if (_lifeSupportOut >= kLifeSupportMillis) {
			speak(0x85, true);
			finish(kDestroyed);
			return;
		}
	}
	if (!e.alive)
		return;
	bool any = false;
	for (size_t i = 1; i < _ships.size(); i++)
		any |= isEnemyAlive(_ships[i]);
	if (any)
		return;
	// The next wave arrives, round the Enterprise (0x4A2FA9).
	int next = 0;
	for (size_t i = 1; i < _ships.size(); i++) {
		if (_ships[i].hidden && _ships[i].alive && (next == 0 || _ships[i].wave < next))
			next = _ships[i].wave;
	}
	if (next) {
		for (size_t i = 1; i < _ships.size(); i++) {
			Ship &s = _ships[i];
			if (s.hidden && s.wave == next) {
				s.hidden = false;
				s.position = s.position + e.position;
			}
		}
		nextTarget(true);
		return;
	}
	speak(0xB7, true);
	finish(kWon);
}

void SpaceCombat::finish(Result result) {
	if (_phase != kFight)
		return;
	_result = result;
	_phase = kEnding;
	_endTime = 0;
	playMusic(result == kWon ? "SCWIN.WAV" : "SCLOSE.WAV", false);
}

// Sound

void SpaceCombat::speak(int id, bool force) {
	Common::String file = speechFile(id);
	if (file.empty() || !_mixer)
		return;
	// Each line waits a while before it can be said again.
	auto said = _spoken.find(id);
	if (!force && said != _spoken.end() && _time - said->second < 8000)
		return;
	if (!force && _mixer->isSoundHandleActive(_speech))
		return;
	std::unique_ptr<Audio::RewindableAudioStream> stream = _resources.loadSound({ kArchive, file });
	if (!stream)
		return;
	_spoken[id] = _time;
	_mixer->stopHandle(_speech);
	_mixer->playStream(Audio::Mixer::kSpeechSoundType, &_speech, std::move(stream));
	_subtitle = _resources.loadSubtitle({ kArchive, file });
	_subtitleLeft = 2500;
	if (!_subtitle.empty())
		info("%s", _subtitle.c_str());
}

void SpaceCombat::playEffect(const Common::String &name, int volume) {
	if (!_mixer)
		return;
	std::unique_ptr<Audio::RewindableAudioStream> stream = _resources.loadSound({ kArchive, name });
	if (stream)
		_mixer->playStream(Audio::Mixer::kSFXSoundType, &_effect, std::move(stream), -1, (byte)std::max(0, std::min(255, volume)));
}

void SpaceCombat::playMusic(const Common::String &name, bool loop) {
	if (!_mixer)
		return;
	_mixer->stopHandle(_music);
	std::unique_ptr<Audio::RewindableAudioStream> stream = _resources.loadSound({ kArchive, name });
	if (!stream)
		return;
	if (loop)
		_mixer->playStream(Audio::Mixer::kMusicSoundType, &_music, Audio::makeLoopingAudioStream(std::move(stream), 0));
	else
		_mixer->playStream(Audio::Mixer::kMusicSoundType, &_music, std::move(stream));
}

// Input

void SpaceCombat::keyDown(const Common::KeyState &key) {
	if (_phase == kBriefing) {
		if (key.keycode == Common::KEYCODE_RETURN || key.keycode == Common::KEYCODE_SPACE ||
		    key.keycode == Common::KEYCODE_ESCAPE)
			mouseUp(_proceedX + 10, _proceedY + 10);
		return;
	}
	if (_phase == kEnding) {
		if (key.keycode == Common::KEYCODE_ESCAPE || key.keycode == Common::KEYCODE_SPACE ||
		    key.keycode == Common::KEYCODE_RETURN)
			_finished = true;
		return;
	}
	Helm before = _helm;
	auto manual = [this]() {
		// Flying by hand puts the helm on stand by (0x49557B).
		if (_helm != kStandBy) {
			_helm = kStandBy;
			speak(0xB3);
		}
	};
	switch (key.keycode) {
	case Common::KEYCODE_ESCAPE:
		_optionsRequest = true;
		break;
	case Common::KEYCODE_LSHIFT:
	case Common::KEYCODE_RSHIFT:
	case Common::KEYCODE_f:
		if (_phasersOnline && !_phaserLockout) {
			_phaserLockout = 1000;
			speak(0xAD);
			firePhasers(0);
		}
		break;
	case Common::KEYCODE_RETURN:
	case Common::KEYCODE_KP_ENTER:
	case Common::KEYCODE_g:
		if (_photonsOnline && !_photonLockout) {
			_photonLockout = 1000;
			speak(0xAE);
			firePhotons(0);
		}
		break;
	case Common::KEYCODE_LEFT:
	case Common::KEYCODE_KP4:
		manual();
		_turnKeys[0] = -1;
		break;
	case Common::KEYCODE_RIGHT:
	case Common::KEYCODE_KP6:
		manual();
		_turnKeys[0] = 1;
		break;
	case Common::KEYCODE_UP:
	case Common::KEYCODE_KP8:
		manual();
		_turnKeys[1] = -1; // dive, as a flight stick
		break;
	case Common::KEYCODE_DOWN:
	case Common::KEYCODE_KP2:
		manual();
		_turnKeys[1] = 1;
		break;
	case Common::KEYCODE_LCTRL:
	case Common::KEYCODE_RCTRL:
	case Common::KEYCODE_q:
		manual();
		_turnKeys[2] = -1;
		break;
	case Common::KEYCODE_KP0:
	case Common::KEYCODE_e:
		manual();
		_turnKeys[2] = 1;
		break;
	case Common::KEYCODE_KP_PLUS:
	case Common::KEYCODE_EQUALS:
		manual();
		_speedKey = 1;
		break;
	case Common::KEYCODE_KP_MINUS:
	case Common::KEYCODE_MINUS:
		manual();
		_speedKey = -1;
		break;
	case Common::KEYCODE_KP5:
		_helm = kFullStop;
		break;
	case Common::KEYCODE_TAB:
	case Common::KEYCODE_t:
		nextTarget(true);
		break;
	case Common::KEYCODE_v:
		_enemyView = !_enemyView;
		speak(_enemyView ? 0x12A : 0x129);
		break;
	default:
		if (key.keycode >= Common::KEYCODE_1 && key.keycode <= Common::KEYCODE_7)
			_helm = (Helm)(key.keycode - Common::KEYCODE_1);
		break;
	}
	if (_helm != before) {
		speak(0xAF + _helm, true);
		if (_helm == kMaintain && _target >= 0)
			_keepRange = (_ships[_target].position - _ships[0].position).length();
		if (_helm == kWarpOut) {
			// Warp needs a fifth of the engines (0x4A53A1).
			if (systemFraction(_ships[0], ShipClass::kEngines) >= 0.21) {
				finish(kLost);
			} else {
				speak(0x9E, true);
				_helm = kStandBy;
			}
		}
	}
}

void SpaceCombat::keyUp(const Common::KeyState &key) {
	switch (key.keycode) {
	case Common::KEYCODE_LEFT:
	case Common::KEYCODE_RIGHT:
	case Common::KEYCODE_KP4:
	case Common::KEYCODE_KP6:
		_turnKeys[0] = 0;
		break;
	case Common::KEYCODE_UP:
	case Common::KEYCODE_DOWN:
	case Common::KEYCODE_KP8:
	case Common::KEYCODE_KP2:
		_turnKeys[1] = 0;
		break;
	case Common::KEYCODE_LCTRL:
	case Common::KEYCODE_RCTRL:
	case Common::KEYCODE_KP0:
	case Common::KEYCODE_q:
	case Common::KEYCODE_e:
		_turnKeys[2] = 0;
		break;
	case Common::KEYCODE_KP_PLUS:
	case Common::KEYCODE_KP_MINUS:
	case Common::KEYCODE_EQUALS:
	case Common::KEYCODE_MINUS:
		_speedKey = 0;
		break;
	default:
		break;
	}
}

bool SpaceCombat::takeOptionsRequest() {
	bool request = _optionsRequest;
	_optionsRequest = false;
	return request;
}

void SpaceCombat::mouseDown(int x, int y) {
	_pressed = -1;
	if (_phase == kFight && Common::Rect(202, 380, 307, 400).contains(x, y))
		_pressed = 0;
	else if (_phase == kFight && Common::Rect(320, 380, 425, 400).contains(x, y))
		_pressed = 1;
}

void SpaceCombat::mouseUp(int x, int y) {
	int pressed = _pressed;
	_pressed = -1;
	if (_phase == kBriefing) {
		if (Common::Rect(_proceedX, _proceedY, _proceedX + 83, _proceedY + 19).contains(x, y)) {
			playEffect("PROCEED.WAV");
			if (_mixer)
				_mixer->stopHandle(_music);
			_phase = kFight;
			playMusic(Common::format("SCLOOP%d.WAV", 1 + (int)_random.getRandomNumber(3)), true);
		}
		return;
	}
	if (_phase != kFight)
		return;
	Common::KeyState key;
	if (pressed == 0 && Common::Rect(202, 380, 307, 400).contains(x, y)) {
		key.keycode = Common::KEYCODE_f;
		keyDown(key);
	} else if (pressed == 1 && Common::Rect(320, 380, 425, 400).contains(x, y)) {
		key.keycode = Common::KEYCODE_g;
		keyDown(key);
	} else if (Common::Rect(307, 380, 320, 400).contains(x, y)) {
		_phasersOnline = !_phasersOnline;
		speak(_phasersOnline ? 0x12D : 0x12E, true);
	} else if (Common::Rect(425, 380, 438, 400).contains(x, y)) {
		_photonsOnline = !_photonsOnline;
		speak(_photonsOnline ? 0x12F : 0x130, true);
	} else if (Common::Rect(177, 317, 190, 343).contains(x, y)) {
		nextTarget(true);
	} else if (Common::Rect(177, 344, 190, 372).contains(x, y)) {
		key.keycode = Common::KEYCODE_v;
		keyDown(key);
	} else if (Common::Rect(472, 319, 639, 459).contains(x, y)) {
		// The helm menu (CONNMNU0): a row each 20 pixels.
		int row = (y - 319) / 20;
		if (row >= 0 && row <= 6) {
			key.keycode = (Common::KeyCode)(Common::KEYCODE_1 + row);
			keyDown(key);
		}
	} else if (Common::Rect(1, 252, 175, 462).contains(x, y) && _target >= 0) {
		// The enemy's systems: aim at the next one (weapons .. cloak).
		const ShipClass *cls = _ships[_target].cls;
		for (int k = 0; k < 8; k++) {
			_aimedSystem = _aimedSystem < 5 || _aimedSystem >= 12 ? 5 : _aimedSystem + 1;
			if (cls && cls->systems[_aimedSystem].max > 0)
				break;
		}
		speak(0x120 + _aimedSystem - 5, true);
	}
}

// Drawing

void SpaceCombat::drawImage(Graphics::Surface &screen, const CombatImage *img, int x, int y, const Common::Rect *clip) {
	if (!img)
		return;
	Common::Rect area(screen.w, screen.h);
	if (clip)
		area.clip(*clip);
	for (int sy = 0; sy < img->h; sy++) {
		int dy = y + sy;
		if (dy < area.top || dy >= area.bottom)
			continue;
		uint32 *row = (uint32 *)screen.getBasePtr(0, dy);
		for (int sx = 0; sx < img->w; sx++) {
			int dx = x + sx;
			uint32 c = img->get(sx, sy);
			if (dx >= area.left && dx < area.right && (c >> 24))
				row[dx] = c;
		}
	}
}

void SpaceCombat::drawText(Graphics::Surface &screen, const Common::String &text, int x, int y) {
	const CombatImage *strip = image("SCANNER.RAS");
	if (!strip)
		return;
	for (char ch : text) {
		int c = (byte)ch;
		if (c < 32 || c > 127)
			c = 32;
		const MetFont::Cell &cell = _font.cells[c - 32];
		for (int gy = 0; gy < cell.h; gy++) {
			for (int gx = 0; gx < cell.w; gx++) {
				int sx = cell.x + gx, sy = cell.y + gy, dx = x + gx, dy = y + gy;
				if (sx >= strip->w || sy >= strip->h || dx < 0 || dy < 0 || dx >= screen.w || dy >= screen.h)
					continue;
				uint32 p = strip->get(sx, sy);
				// The strip's glyphs are light on dark.
				if ((p >> 24) && ((p >> 16 & 0xFF) + (p >> 8 & 0xFF) + (p & 0xFF)) > 150)
					*(uint32 *)screen.getBasePtr(dx, dy) = p;
			}
		}
		x += std::max(cell.w, 3) + 1;
		if (x > screen.w)
			break;
	}
}

void SpaceCombat::drawBars(Graphics::Surface &screen, const Ship &ship, int x, int y) {
	// Twelve columns, shields to cloak: the dark covers what's lost.
	if (!ship.cls)
		return;
	for (int s = 1; s <= 12; s++) {
		int max = ship.cls->systems[s].max;
		if (max <= 0)
			continue;
		double level = s <= 4 ? ship.energy[s] / max : systemFraction(ship, s);
		level = std::max(0.0, std::min(1.0, level));
		int left = x + 1 + 14 * (s - 1), top = y + 9, height = 41;
		int dark = (int)(height * (1 - level));
		for (int dy = 0; dy < dark; dy++) {
			uint32 *row = (uint32 *)screen.getBasePtr(0, top + dy);
			for (int dx = 1; dx < 12; dx++)
				row[left + dx] = darken(row[left + dx], 0.25);
		}
	}
}

void SpaceCombat::drawRadar(Graphics::Surface &screen) {
	// Top down round the Enterprise, which faces up the screen.
	const int cx = 321, cy = 323;
	drawImage(screen, image("RADARM.RAS"), cx - 102, cy - 48);
	const Ship &e = _ships[0];
	double range = 40000;
	for (size_t i = 0; i < _ships.size(); i++) {
		const Ship &s = _ships[i];
		if (!isEnemyAlive(s) && i != 0)
			continue;
		Vec3 local = e.orientation.inverse(s.position - e.position);
		int x = cx + (int)(local.x / range * 100), y = cy - (int)(local.z / range * 46);
		if (x < 196 || x > 444 || y < 255 || y > 372)
			continue;
		Common::String token = Common::toUpper(s.token);
		const CombatImage *blip = image(Common::format("S%sMAP%s.RAS", token.c_str(), (int)i == _target ? "H" : ""));
		if (!blip)
			blip = image(Common::format("S%sMAP.RAS", token.c_str()));
		if (blip)
			drawImage(screen, blip, x - blip->w / 2, y - blip->h / 2);
	}
	if (_target >= 0 && _target < (int)_ships.size())
		drawText(screen, Common::format("RANGE: %6.0f", (_ships[_target].position - e.position).length()), 209, 362);
}

void SpaceCombat::drawView(Graphics::Surface &screen) {
	const Ship &e = _ships[0];
	// The camera: behind the Enterprise, or on the line to the target.
	Vec3 camPos;
	Mat3 camRot;
	if (_enemyView && _target >= 0 && isEnemyAlive(_ships[_target])) {
		Vec3 dir = (_ships[_target].position - e.position).normalized();
		camPos = e.position - dir * (e.radius * 3) + e.orientation.up * (e.radius * 0.6);
		camRot = Mat3::lookAlong(_ships[_target].position - camPos, e.orientation.up);
	} else {
		camRot = e.orientation;
		camPos = e.position - e.orientation.forward * (e.radius * 3.2) + e.orientation.up * (e.radius * 0.7);
	}
	_renderer.setCamera(camPos, camRot);
	Common::Rect view(kViewX, kViewY, kViewX + kViewW, kViewY + kViewH);
	for (int y = view.top; y < view.bottom; y++) {
		uint32 *row = (uint32 *)screen.getBasePtr(0, y);
		std::fill(row + view.left, row + view.right, 0xFF000000u);
	}
	_renderer.begin(screen, kViewX, kViewY);

	// The stars, far away: only the camera's turn moves them.
	for (const Vec3 &star : _stars) {
		Vec3 local = camRot.inverse(star);
		if (local.z <= 0.05)
			continue;
		int x = (int)(kViewW / 2.0 + _renderer.getFocal() * local.x / local.z);
		int y = (int)(kViewH / 2.0 - _renderer.getFocal() * local.y / local.z);
		_renderer.plot(x, y, (int)(std::fabs(star.x) * 1000) % 3 ? 0xFF9090A0u : 0xFFE0E0FFu);
	}

	auto lookup = [this](const Common::String &name) { return texture(name); };
	for (size_t i = 0; i < _ships.size(); i++) {
		const Ship &s = _ships[i];
		if (!s.alive || s.hidden || s.gone)
			continue;
		double distance = (s.position - camPos).length();
		// The detailed model near, the rough one further off (0x54E5D8).
		bool near = distance < std::max(6000.0, s.radius * 40);
		const Model3D *m = near || !s.lowModel ? s.model : s.lowModel;
		const std::vector<CombatMaterial> *mats = near || !s.lowModel ? s.materials : s.lowMaterials;
		if (m && mats) {
			_renderer.drawModel(*m, *mats, lookup, s.position, s.orientation);
		} else {
			double sx, sy, depth;
			if (_renderer.project(s.position, sx, sy, depth))
				_renderer.plot((int)sx, (int)sy, 0xFFFFFFFFu);
		}
		// The target's box (0x4A2AD2).
		if ((int)i == _target) {
			double sx, sy, depth;
			if (_renderer.project(s.position, sx, sy, depth)) {
				int r = std::max(6, (int)(s.radius * _renderer.getFocal() / depth));
				for (int k = -r; k <= r; k++) {
					if (std::abs(k) < r / 2)
						continue;
					_renderer.plot((int)sx + k, (int)sy - r, 0xFFFFE040u);
					_renderer.plot((int)sx + k, (int)sy + r, 0xFFFFE040u);
					_renderer.plot((int)sx - r, (int)sy + k, 0xFFFFE040u);
					_renderer.plot((int)sx + r, (int)sy + k, 0xFFFFE040u);
				}
			}
		}
	}
	for (const Beam &b : _beams) {
		Vec3 from = _ships[b.from].position;
		_renderer.drawLine(from, b.end, b.colour, 2);
	}
	for (const Torpedo &t : _torpedoes) {
		const SprAnimation *spr = t.sprite;
		if (!spr || spr->frames.empty())
			continue;
		const std::vector<byte> &frame = spr->frames[(_time / spr->frameMillis) % spr->frames.size()];
		_renderer.drawBillboard(t.position, 220, spr->w, spr->h,
		                        [&](int x, int y) {
			                        byte v = frame[(size_t)y * spr->w + x];
			                        return v ? _palette[v] : 0u;
		                        },
		                        true);
	}
	for (const Explosion &e2 : _explosions) {
		const SprAnimation *spr = e2.sprite;
		if (!spr || spr->frames.empty())
			continue;
		size_t index = std::min(spr->frames.size() - 1, (size_t)(e2.time / spr->frameMillis));
		const std::vector<byte> &frame = spr->frames[index];
		_renderer.drawBillboard(e2.position, e2.size, spr->w, spr->h,
		                        [&](int x, int y) {
			                        byte v = frame[(size_t)y * spr->w + x];
			                        return v ? _palette[v] : 0u;
		                        },
		                        true);
	}
}

void SpaceCombat::drawConsole(Graphics::Surface &screen) {
	drawImage(screen, image("COMBAT.RAS"), 0, 0);
	const Ship &e = _ships[0];

	// Left, the target; right, the Enterprise and the helm menu.
	auto state = [this](const Ship &s) {
		int destroyed = 0;
		if (s.cls)
			for (int k = ShipClass::kWeapons; k <= ShipClass::kCloak; k++)
				destroyed += s.cls->systems[k].max > 0 && s.damage[k] >= s.cls->systems[k].max;
		return destroyed == 0 ? 0 : destroyed < 3 ? 1 : 2;
	};
	if (_target >= 0 && _target < (int)_ships.size() && isEnemyAlive(_ships[_target])) {
		const Ship &t = _ships[_target];
		Common::String token = Common::toUpper(t.token);
		if (systemFraction(e, ShipClass::kSensors) <= 0) {
			drawImage(screen, image(Common::format("STATIC%d.RAS", 1 + (int)(_time / 100) % 3)), 0, 252);
		} else {
			drawImage(screen, image(Common::format("S%s_%d.RAS", token.c_str(), state(t))), 0, 252);
			drawBars(screen, t, 0, 252);
		}
	}
	drawImage(screen, image(Common::format("SENT_%d.RAS", state(e))), 466, 255);
	drawBars(screen, e, 466, 255);
	const CombatImage *menu = image("CONNMNU0.RAS"), *lit = image("CONNMNU1.RAS");
	drawImage(screen, menu, 472, 310);
	if (lit && _helm <= kMaintainStatus) {
		Common::Rect band(472, 310 + _helm * 20 + 9, 472 + lit->w, 310 + _helm * 20 + 29);
		drawImage(screen, lit, 472, 310, &band);
	}

	// The fire buttons: 0 dim, 1 lit, 2 normal.
	bool target = _target >= 0;
	drawImage(screen, image(!_phasersOnline || !target ? "BPHASER0.RAS" : _pressed == 0 ? "BPHASER1.RAS" : "BPHASER2.RAS"), 202, 379);
	drawImage(screen, image(!_photonsOnline || !target ? "BPHOTON0.RAS" : _pressed == 1 ? "BPHOTON1.RAS" : "BPHOTON2.RAS"), 320, 379);
	drawImage(screen, image(_phasersOnline ? "BPHASERN.RAS" : "BPHASERF.RAS"), 307, 380);
	drawImage(screen, image(_photonsOnline ? "BPHOTONN.RAS" : "BPHOTONF.RAS"), 425, 380);
	drawImage(screen, image(_enemyView ? "BENEMYN.RAS" : "BENEMYF.RAS"), 174, 344);

	// The tubes, and the torpedoes left.
	if (e.cls) {
		for (int t = 0; t < 2; t++) {
			bool loaded = e.loaded[t] > 0;
			drawImage(screen, image(loaded ? "TUBEBLUE.RAS" : "TUBERED.RAS"), t == 0 ? 321 : 399, 402);
		}
		drawText(screen, Common::format("%3d", e.stock + e.loaded[0] + e.loaded[1]), 362, 410);
		// The phasers' charge.
		const CombatImage *meter = image("PHASERPW.RAS");
		if (meter) {
			drawImage(screen, meter, 203, 402);
			double charge = 0, max = 0;
			for (int s = ShipClass::kPhaser0; s < ShipClass::kPhoton0; s++) {
				if (e.cls->systems[s].max > 0) {
					charge += e.energy[s];
					max += e.cls->systems[s].max;
				}
			}
			int cover = max > 0 ? (int)(meter->w * (1 - charge / max)) : meter->w;
			for (int y = 402; y < 402 + meter->h; y++) {
				uint32 *row = (uint32 *)screen.getBasePtr(0, y);
				for (int x = 203 + meter->w - cover; x < 203 + meter->w; x++)
					row[x] = darken(row[x], 0.3);
			}
		}
	}
	drawRadar(screen);

	// The message line: subtitles, else the helm and speed.
	Common::String line;
	if (_subtitles && !_subtitle.empty())
		line = _subtitle;
	else if (_phase == kEnding)
		line = _result == kWon ? "VICTORY" : _result == kLost ? "WARPED OUT OF THE SYSTEM" : "THE ENTERPRISE IS LOST";
	else
		line = Common::format("SPEED %4.0f", e.speed);
	drawText(screen, line.substr(0, 110), 4, 462);
}

void SpaceCombat::draw(Graphics::Surface &screen) {
	if (_phase == kBriefing) {
		for (int y = 0; y < screen.h; y++) {
			uint32 *row = (uint32 *)screen.getBasePtr(0, y);
			std::fill(row, row + screen.w, 0xFF000000u);
		}
		_briefing.draw(screen, 0, (screen.w - _briefing.getWidth()) / 2, (screen.h - _briefing.getFrameHeight()) / 2);
		_proceed.draw(screen, _pressed == 2 ? 1 : 0, _proceedX, _proceedY);
		return;
	}
	drawView(screen);
	drawConsole(screen);
}

} // End of namespace STG
