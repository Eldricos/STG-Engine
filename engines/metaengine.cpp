/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/metaengine.h"

// Each enabled engine provides a factory for its MetaEngine. CMake defines
// ENABLE_<ENGINE> for every engine that is built in.
#ifdef ENABLE_STG
const MetaEngine *getSTGMetaEngine();
#endif

const std::vector<const MetaEngine *> &getMetaEngines() {
	static const std::vector<const MetaEngine *> engines = {
#ifdef ENABLE_STG
		getSTGMetaEngine(),
#endif
	};
	return engines;
}

const MetaEngine *findMetaEngine(const Common::String &id) {
	for (const MetaEngine *engine : getMetaEngines()) {
		if (Common::equalsIgnoreCase(engine->getId(), id))
			return engine;
	}
	return nullptr;
}
