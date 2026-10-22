/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/ini.h"

#include <cstdlib>

#include "common/serializer.h"

namespace STG {

void IniFile::load(const Common::String &text) {
	_sections.clear();
	_changes.clear();
	Section *section = nullptr;
	for (Common::String line : Common::split(text, '\n')) {
		line = Common::trim(line);
		if (line.empty() || line[0] == ';')
			continue;
		if (line[0] == '[') {
			size_t end = line.find(']');
			section = &_sections[Common::toLower(line.substr(1, end == Common::String::npos ? end : end - 1))];
			continue;
		}
		size_t eq = line.find('=');
		if (!section || eq == Common::String::npos)
			continue;
		Common::String key = Common::toLower(Common::trim(line.substr(0, eq)));
		// The first copy of a key wins, as the Windows lookups find it.
		if (!section->count(key))
			(*section)[key] = Common::trim(line.substr(eq + 1));
	}
}

bool IniFile::hasSection(const Common::String &section) const {
	return _sections.count(Common::toLower(section)) != 0;
}

bool IniFile::has(const Common::String &section, const Common::String &key) const {
	auto s = _sections.find(Common::toLower(section));
	return s != _sections.end() && s->second.count(Common::toLower(key));
}

Common::String IniFile::get(const Common::String &section, const Common::String &key,
                            const Common::String &fallback) const {
	auto s = _sections.find(Common::toLower(section));
	if (s == _sections.end())
		return fallback;
	auto k = s->second.find(Common::toLower(key));
	return k == s->second.end() ? fallback : k->second;
}

int IniFile::getInt(const Common::String &section, const Common::String &key, int fallback) const {
	return has(section, key) ? std::atoi(get(section, key).c_str()) : fallback;
}

double IniFile::getDouble(const Common::String &section, const Common::String &key, double fallback) const {
	return has(section, key) ? std::atof(get(section, key).c_str()) : fallback;
}

bool IniFile::getBool(const Common::String &section, const Common::String &key, bool fallback) const {
	if (!has(section, key))
		return fallback;
	Common::String v = Common::toLower(get(section, key));
	return v == "true" || v == "yes" || std::atoi(v.c_str()) != 0;
}

void IniFile::set(const Common::String &section, const Common::String &key, const Common::String &value) {
	_sections[Common::toLower(section)][Common::toLower(key)] = value;
	_changes[{ Common::toLower(section), Common::toLower(key) }] = value;
}

void IniFile::syncChanges(Common::Serializer &s) {
	std::vector<std::pair<Common::String, Common::String>> keys;
	std::vector<Common::String> values;
	for (const auto &change : _changes) {
		keys.push_back(change.first);
		values.push_back(change.second);
	}
	uint32 count = (uint32)keys.size();
	s.syncAsUint32(count);
	if (s.isLoading()) {
		if (count > 10000) {
			s.fail();
			return;
		}
		keys.resize(count);
		values.resize(count);
	}
	for (uint32 i = 0; i < count && !s.err(); i++) {
		s.syncString(keys[i].first);
		s.syncString(keys[i].second);
		s.syncString(values[i]);
	}
	if (s.isLoading() && !s.err()) {
		for (uint32 i = 0; i < count; i++)
			set(keys[i].first, keys[i].second, values[i]);
	}
}

} // End of namespace STG
