/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_CONFIG_MANAGER_H
#define COMMON_CONFIG_MANAGER_H

#include <map>

#include "common/fs.h"
#include "common/str.h"

namespace Common {

/**
 * INI-style settings, organised like ScummVM's: a global section plus one
 * section per configured game target. Lookups fall through
 * transient (command line) -> active game -> global -> registered defaults.
 */
class ConfigManager {
public:
	static const char *const kApplicationDomain;

	bool loadFromFile(const FSNode &file);
	bool saveToFile(const FSNode &file) const;
	bool loadDefaultConfigFile(const FSNode &file);
	bool flushToDisk() const;

	void setActiveDomain(const String &domain) { _activeDomain = domain; }
	const String &getActiveDomain() const { return _activeDomain; }

	bool hasKey(const String &key) const;
	String get(const String &key) const;
	int getInt(const String &key) const;
	bool getBool(const String &key) const;

	/** Set in the active game domain, or the global one if none is active. */
	void set(const String &key, const String &value);
	void setInt(const String &key, int value);
	void setBool(const String &key, bool value);

	/** Set for this run only; never written to disk. */
	void setTransient(const String &key, const String &value);

	void set(const String &key, const String &value, const String &domain);
	String get(const String &key, const String &domain) const;

	void registerDefault(const String &key, const String &value);
	void registerDefault(const String &key, int value);
	void registerDefault(const String &key, bool value);

	bool hasDomain(const String &domain) const { return _domains.count(domain) != 0; }
	void removeDomain(const String &domain) { _domains.erase(domain); }
	std::vector<String> getGameDomains() const;

private:
	typedef std::map<String, String> Domain;

	const String *find(const Domain &domain, const String &key) const;

	std::map<String, Domain> _domains;
	Domain _transient;
	Domain _defaults;
	String _activeDomain;
	FSNode _file;
};

ConfigManager &getConfigManager();

} // End of namespace Common

#define ConfMan (Common::getConfigManager())

#endif
