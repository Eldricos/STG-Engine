/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/campaign.h"

#include <cstdlib>

#include "common/debug.h"
#include "common/serializer.h"

namespace STG {

namespace {

int toInt(const std::vector<Common::String> &words, size_t i) {
	return i < words.size() ? std::atoi(words[i].c_str()) : 0;
}

Common::String word(const std::vector<Common::String> &words, size_t i) {
	return i < words.size() ? Common::toLower(words[i]) : Common::String();
}

template<typename T>
void syncSet(Common::Serializer &s, std::set<T> &values, void (*sync)(Common::Serializer &, T &)) {
	std::vector<T> list(values.begin(), values.end());
	s.syncVector(list, sync);
	if (s.isLoading())
		values = std::set<T>(list.begin(), list.end());
}

void syncInt(Common::Serializer &s, int &v) { s.syncAsInt(v); }
void syncString(Common::Serializer &s, Common::String &v) { s.syncString(v); }

} // End of anonymous namespace

bool Campaign::load(const Common::String &text) {
	_sections.clear();
	std::vector<std::vector<Common::String>> *section = nullptr;
	for (Common::String line : Common::split(text, '\n')) {
		size_t comment = line.find(';');
		if (comment != Common::String::npos)
			line.resize(comment);
		line = Common::trim(line);
		if (line.empty())
			continue;
		if (line[0] == '[') {
			size_t end = line.find(']');
			section = &_sections[Common::toLower(line.substr(1, end == Common::String::npos ? end : end - 1))];
			continue;
		}
		if (!section)
			continue;
		std::vector<Common::String> words;
		size_t i = 0;
		while (i < line.size()) {
			while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
				i++;
			size_t start = i;
			while (i < line.size() && line[i] != ' ' && line[i] != '\t')
				i++;
			if (i > start)
				words.push_back(line.substr(start, i - start));
		}
		section->push_back(words);
	}
	if (!_sections.count("start")) {
		_sections.clear();
		return false;
	}
	return true;
}

void Campaign::start(const Common::String &section) {
	_flags.clear();
	_played.clear();
	_won.clear();
	_lost.clear();
	_destroyed.clear();
	_objectFlags.clear();
	_stack.clear();
	_mission = 0;
	_running = true;
	jump(section);
}

void Campaign::jump(const Common::String &section) {
	_section = Common::toLower(section);
	_line = 0;
	if (!_sections.count(_section)) {
		warning("Campaign: no section [%s]", section.c_str());
		_running = false;
	}
}

Campaign::Action Campaign::next() {
	Action action;
	// A bound on lines run without an action, in case of a loop.
	for (int steps = 0; _running && !_mission && steps < 10000; steps++) {
		const std::vector<std::vector<Common::String>> &lines = _sections[_section];
		if (_line >= lines.size()) {
			// The original waits on the bridge here, for the player to
			// travel; with no bridge, nothing more can happen.
			warning("Campaign: [%s] ends without going on", _section.c_str());
			_running = false;
			break;
		}
		const std::vector<Common::String> &words = lines[_line++];
		if (execute(words, action))
			return action;
	}
	return Action();
}

void Campaign::missionEnded(bool accomplished) {
	if (!_mission)
		return;
	_played.insert(_mission);
	(accomplished ? _won : _lost).insert(_mission);
	_mission = 0;
	jump(accomplished ? _wonSection : _lostSection);
}

bool Campaign::test(const std::vector<Common::String> &words, size_t &i) const {
	while (i < words.size()) {
		Common::String w = word(words, i);
		if (w.compare(0, 2, "if") != 0)
			return true;
		bool pass;
		if (w == "ifflag") {
			pass = _flags.count(toInt(words, i + 1)) != 0;
			i += 2;
		} else if (w == "ifnotflag") {
			pass = !_flags.count(toInt(words, i + 1));
			i += 2;
		} else if (w == "ifactiongameplayed") {
			pass = _played.count(toInt(words, i + 1)) != 0;
			i += 2;
		} else if (w == "ifsunsdestroyed") {
			pass = (int)_destroyed.size() == toInt(words, i + 1);
			i += 2;
		} else if (w == "ifenterpriseat") {
			pass = false; // there's no bridge to have moved the ship
			i += 2;
		} else if (w == "ifobjectflag" || w == "ifnotobjectflag") {
			bool set = _objectFlags.count(word(words, i + 1) + " " + word(words, i + 2)) != 0;
			pass = w == "ifobjectflag" ? set : !set;
			i += 3;
		} else if (w == "ifclue") {
			// Which clue this tests isn't known; without it, the endings
			// take their usual way.
			pass = false;
			i += 1;
		} else {
			warning("Campaign: unknown condition %s", words[i].c_str());
			return false;
		}
		if (!pass)
			return false;
	}
	return true;
}

bool Campaign::execute(const std::vector<Common::String> &words, Action &action) {
	size_t i = 0;
	if (!test(words, i) || i >= words.size())
		return false;
	Common::String command = word(words, i);
	size_t a = i + 1; // the first argument

	if (command == "jump") {
		jump(words.size() > a ? words[a] : Common::String());
	} else if (command == "jumprandom") {
		int count = (int)(words.size() - a);
		if (count > 0)
			jump(words[a + (_random ? _random(count) : 0)]);
	} else if (command == "call") {
		_stack.push_back({ _section, (int)_line });
		jump(words.size() > a ? words[a] : Common::String());
	} else if (command == "pop" || command == "return") {
		if (_stack.empty()) {
			warning("Campaign: %s with nothing to return to in [%s]", words[i].c_str(), _section.c_str());
			_running = false;
		} else {
			_section = _stack.back().first;
			_line = _stack.back().second;
			_stack.pop_back();
		}
	} else if (command == "ontransport") {
		// The player would beam down from the bridge: do it now.
		jump(words.size() > a ? words[a] : Common::String());
	} else if (command == "ontactical") {
		// The player could engage a fleet from the bridge. With no space
		// combat, they never do: the section goes on as when the chance
		// passes.
	} else if (command == "spacecombat") {
		info("Campaign: space combat %d isn't implemented; taking it as lost", toInt(words, a));
		jump(word(words, a + 2));
	} else if (command == "actiongame") {
		_mission = toInt(words, a);
		_wonSection = word(words, a + 1);
		_lostSection = word(words, a + 2);
		action.type = Action::kActionGame;
		action.number = _mission;
		return true;
	} else if (command == "setflag") {
		if (Common::equalsIgnoreCase(word(words, a + 1), "true"))
			_flags.insert(toInt(words, a));
		else
			_flags.erase(toInt(words, a));
	} else if (command == "setobjectflag") {
		Common::String key = word(words, a) + " " + word(words, a + 1);
		if (word(words, a + 2) == "true")
			_objectFlags.insert(key);
		else
			_objectFlags.erase(key);
	} else if (command == "setactiongameplayed") {
		_played.insert(toInt(words, a));
	} else if (command == "destroy") {
		_destroyed.insert(word(words, a));
	} else if (command == "intro") {
		action.type = Action::kIntro;
		return true;
	} else if (command == "movie") {
		action.type = Action::kMovie;
		action.number = toInt(words, a);
		return true;
	} else if (command == "scrolltext") {
		action.type = Action::kScrollText;
		action.number = toInt(words, a);
		return true;
	} else if (command == "autosave") {
		action.type = Action::kAutoSave;
		return true;
	} else if (command == "options" || command == "endgame") {
		// Both end the game: the menu with nothing to go back to, or the credits.
		action.type = command == "options" ? Action::kOptions : Action::kEndGame;
		_running = false;
		return true;
	}
	// Anything else drives the bridge: the map, its sounds and timers.
	return false;
}

void Campaign::sync(Common::Serializer &s) {
	s.syncString(_section);
	uint32 line = (uint32)_line;
	s.syncAsUint32(line);
	_line = line;
	s.syncAsBool(_running);
	s.syncAsInt(_mission);
	s.syncString(_wonSection);
	s.syncString(_lostSection);
	syncSet<int>(s, _flags, syncInt);
	syncSet<int>(s, _played, syncInt);
	syncSet<int>(s, _won, syncInt);
	syncSet<int>(s, _lost, syncInt);
	syncSet<Common::String>(s, _destroyed, syncString);
	syncSet<Common::String>(s, _objectFlags, syncString);
	s.syncVector(_stack, [](Common::Serializer &ser, std::pair<Common::String, int> &entry) {
		ser.syncString(entry.first);
		ser.syncAsInt(entry.second);
	});
	if (s.isLoading() && _running && !_sections.count(_section))
		s.fail();
}

} // End of namespace STG
