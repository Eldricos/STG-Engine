/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/locations.h"

#include <algorithm>

#include "common/str.h"

namespace STG {

const std::vector<Location> &getLocations() {
	static const std::vector<Location> kLocations = {
		{ "AMARGOS1", 1 }, { "AMARGOS2", 14 }, { "ANTILIOS", 6 }, { "ARVADA1", 2 }, { "ARVADA2", 8 },
		{ "BERSUS", 7 },   { "ENTERP", 13 },   { "EPSION", 10 },  { "GUERN", 11 },   { "HALEE1", 3 },
		{ "HALEE2", 4 },   { "QURASH", 5 },    { "GALORN1", 9 },  { "GALORN2", 15 }, { "VERID1", 12 }
	};
	return kLocations;
}

bool findLocationLevel(const ResourceManager &resources, const Location &location, ResourceRef &ref) {
	Common::String wanted = Common::toLower(Common::String(location.folder) + "/game.lv2");
	for (const ResourceRef &image : resources.getImages()) {
		Common::String name = Common::toLower(image.name);
		std::replace(name.begin(), name.end(), '\\', '/');
		if (name == wanted) {
			ref = image;
			return true;
		}
	}
	return false;
}

} // End of namespace STG
