/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef ENGINES_METAENGINE_H
#define ENGINES_METAENGINE_H

#include <memory>
#include <vector>

#include "common/error.h"
#include "common/fs.h"
#include "common/stream.h"
#include "engines/game.h"

class Engine;
class OSystem;

/**
 * Describes an engine and creates instances of it: the counterpart of
 * ScummVM's MetaEngine. Each engine directory provides one.
 */
class MetaEngine {
public:
	virtual ~MetaEngine() {}

	/** Short id used on the command line, e.g. "stg". */
	virtual const char *getId() const = 0;
	virtual const char *getName() const = 0;
	virtual const char *getOriginalCopyright() const = 0;

	virtual std::vector<PlainGameDescriptor> getSupportedGames() const = 0;

	/** Look for supported games in a directory. */
	virtual DetectedGames detectGames(const Common::FSNode &dir) const = 0;

	/**
	 * Describe a file in one of this engine's own formats for --probe.
	 * verbose is set when a single file is probed. Returns false if the
	 * file isn't one the engine knows.
	 */
	virtual bool describeFile(Common::SeekableReadStream &stream, bool verbose, Common::String &out) const {
		return false;
	}

	/** Register this engine's default configuration values. */
	virtual void registerDefaultSettings() const {}

	virtual Common::Error createInstance(OSystem *syst, const DetectedGame &game,
	                                     std::unique_ptr<Engine> &engine) const = 0;
};

/** All engines compiled into this build. */
const std::vector<const MetaEngine *> &getMetaEngines();

const MetaEngine *findMetaEngine(const Common::String &id);

#endif
