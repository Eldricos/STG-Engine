/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_INI_H
#define STG_INI_H

#include <map>

#include "common/str.h"

namespace Common {
class Serializer;
}

namespace STG {

/**
 * A Windows-style INI file, looked up as GetPrivateProfileString does:
 * section and key names ignore case, and a section that appears twice
 * (UNIVERSE.INI repeats most of them at the end, with the scan texts)
 * holds the keys of both.
 */
class IniFile {
public:
	void load(const Common::String &text);

	bool hasSection(const Common::String &section) const;
	bool has(const Common::String &section, const Common::String &key) const;
	Common::String get(const Common::String &section, const Common::String &key,
	                   const Common::String &fallback = Common::String()) const;
	int getInt(const Common::String &section, const Common::String &key, int fallback = 0) const;
	double getDouble(const Common::String &section, const Common::String &key, double fallback = 0) const;
	/** TRUE/FALSE (or a number); fallback when missing. */
	bool getBool(const Common::String &section, const Common::String &key, bool fallback = false) const;
	void set(const Common::String &section, const Common::String &key, const Common::String &value);

	/** Changes made with set(), which saved games keep. */
	void syncChanges(Common::Serializer &s);

private:
	typedef std::map<Common::String, Common::String> Section;
	std::map<Common::String, Section> _sections;
	std::map<std::pair<Common::String, Common::String>, Common::String> _changes;
};

} // End of namespace STG

#endif
