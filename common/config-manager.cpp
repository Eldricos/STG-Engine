/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "common/config-manager.h"
#include "common/debug.h"

#include <cstdlib>

namespace Common {

const char *const ConfigManager::kApplicationDomain = "stg-engine";

bool ConfigManager::loadFromFile(const FSNode &file) {
	std::unique_ptr<SeekableReadStream> stream = file.createReadStream();
	if (!stream)
		return false;

	String section = kApplicationDomain;
	int lineNum = 0;
	while (!stream->eos()) {
		String line = trim(stream->readLine());
		lineNum++;
		if (line.empty() || line[0] == '#' || line[0] == ';')
			continue;

		if (line[0] == '[') {
			size_t end = line.find(']');
			if (end == String::npos) {
				warning("Config file %s line %d: missing ']'", file.getPath().c_str(), lineNum);
				continue;
			}
			section = trim(line.substr(1, end - 1));
			_domains[section];
			continue;
		}

		size_t eq = line.find('=');
		if (eq == String::npos) {
			warning("Config file %s line %d: expected key=value", file.getPath().c_str(), lineNum);
			continue;
		}
		_domains[section][trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
	}
	return true;
}

bool ConfigManager::saveToFile(const FSNode &file) const {
	std::unique_ptr<WriteStream> stream = file.createWriteStream();
	if (!stream)
		return false;

	auto writeDomain = [&stream](const String &name, const Domain &domain) {
		stream->writeString("[" + name + "]\n");
		for (const auto &kv : domain)
			stream->writeString(kv.first + "=" + kv.second + "\n");
		stream->writeString("\n");
	};

	// Global settings first, then each game target.
	auto app = _domains.find(kApplicationDomain);
	writeDomain(kApplicationDomain, app != _domains.end() ? app->second : Domain());
	for (const auto &domain : _domains) {
		if (domain.first != kApplicationDomain)
			writeDomain(domain.first, domain.second);
	}
	return stream->flush();
}

bool ConfigManager::loadDefaultConfigFile(const FSNode &file) {
	_file = file;
	if (!file.exists())
		return true;
	return loadFromFile(file);
}

bool ConfigManager::flushToDisk() const {
	if (!_file.isValid())
		return false;
	return saveToFile(_file);
}

const String *ConfigManager::find(const Domain &domain, const String &key) const {
	auto it = domain.find(key);
	return it == domain.end() ? nullptr : &it->second;
}

bool ConfigManager::hasKey(const String &key) const {
	if (find(_transient, key))
		return true;
	if (!_activeDomain.empty()) {
		auto it = _domains.find(_activeDomain);
		if (it != _domains.end() && find(it->second, key))
			return true;
	}
	auto app = _domains.find(kApplicationDomain);
	return app != _domains.end() && find(app->second, key);
}

String ConfigManager::get(const String &key) const {
	if (const String *value = find(_transient, key))
		return *value;
	if (!_activeDomain.empty()) {
		auto it = _domains.find(_activeDomain);
		if (it != _domains.end()) {
			if (const String *value = find(it->second, key))
				return *value;
		}
	}
	auto app = _domains.find(kApplicationDomain);
	if (app != _domains.end()) {
		if (const String *value = find(app->second, key))
			return *value;
	}
	if (const String *value = find(_defaults, key))
		return *value;
	return String();
}

int ConfigManager::getInt(const String &key) const {
	return atoi(get(key).c_str());
}

bool ConfigManager::getBool(const String &key) const {
	String value = toLower(get(key));
	return value == "true" || value == "yes" || value == "1" || value == "on";
}

void ConfigManager::set(const String &key, const String &value) {
	set(key, value, _activeDomain.empty() ? kApplicationDomain : _activeDomain);
}

void ConfigManager::setInt(const String &key, int value) {
	set(key, format("%d", value));
}

void ConfigManager::setBool(const String &key, bool value) {
	set(key, value ? "true" : "false");
}

void ConfigManager::setTransient(const String &key, const String &value) {
	_transient[key] = value;
}

void ConfigManager::set(const String &key, const String &value, const String &domain) {
	_domains[domain][key] = value;
	// An explicit set overrides any earlier command-line value for this run.
	_transient.erase(key);
}

String ConfigManager::get(const String &key, const String &domain) const {
	auto it = _domains.find(domain);
	if (it == _domains.end())
		return String();
	const String *value = find(it->second, key);
	return value ? *value : String();
}

void ConfigManager::registerDefault(const String &key, const String &value) {
	_defaults[key] = value;
}

void ConfigManager::registerDefault(const String &key, int value) {
	_defaults[key] = format("%d", value);
}

void ConfigManager::registerDefault(const String &key, bool value) {
	_defaults[key] = value ? "true" : "false";
}

std::vector<String> ConfigManager::getGameDomains() const {
	std::vector<String> result;
	for (const auto &domain : _domains) {
		if (domain.first != kApplicationDomain)
			result.push_back(domain.first);
	}
	return result;
}

ConfigManager &getConfigManager() {
	static ConfigManager configManager;
	return configManager;
}

} // End of namespace Common
