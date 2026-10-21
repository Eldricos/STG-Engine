/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/game.h"

const char *getPlatformDescription(GamePlatform platform) {
	switch (platform) {
	case kPlatformDOS:
		return "DOS";
	case kPlatformWindows:
		return "Windows";
	default:
		return "Unknown";
	}
}

const char *getLanguageDescription(GameLanguage language) {
	switch (language) {
	case kLangEnglish:
		return "English";
	case kLangGerman:
		return "German";
	case kLangFrench:
		return "French";
	case kLangItalian:
		return "Italian";
	case kLangSpanish:
		return "Spanish";
	default:
		return "Unknown";
	}
}

Common::String DetectedGame::getFullDescription() const {
	Common::String desc = description;
	Common::StringArray parts;
	if (!extra.empty())
		parts.push_back(extra);
	if (platform != kPlatformUnknown)
		parts.push_back(getPlatformDescription(platform));
	if (language != kLangUnknown)
		parts.push_back(getLanguageDescription(language));
	if (unknownVariant)
		parts.push_back("unknown version");

	if (!parts.empty()) {
		desc += " (";
		for (size_t i = 0; i < parts.size(); i++) {
			if (i)
				desc += "/";
			desc += parts[i];
		}
		desc += ")";
	}
	return desc;
}

Common::String DetectedGame::getPreferredTarget() const {
	Common::String target = gameId;
	if (platform == kPlatformDOS)
		target += "-dos";
	else if (platform == kPlatformWindows)
		target += "-win";
	return target;
}
