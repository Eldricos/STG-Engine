/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_RESOURCES_H
#define STG_RESOURCES_H

#include <memory>

#include "common/audio/audiostream.h"
#include "common/image/bmp.h"
#include "common/str.h"

namespace STG {

/**
 * Index of the game's resources.
 *
 * For now this only knows standard Windows formats (BMP, WAV) found as
 * loose files. Readers for the game's own container formats belong here
 * as they are reverse engineered; register them with SearchMan so the rest
 * of the engine can keep looking resources up by name.
 */
class ResourceManager {
public:
	void scan();

	const Common::StringArray &getImages() const { return _images; }
	const Common::StringArray &getSounds() const { return _sounds; }

	/** Log a summary of what the game directory contains, by extension. */
	void logInventory() const;

	bool loadImage(const Common::String &name, Image::BitmapDecoder &decoder) const;
	std::unique_ptr<Audio::RewindableAudioStream> loadSound(const Common::String &name) const;

private:
	Common::StringArray _images;
	Common::StringArray _sounds;
};

} // End of namespace STG

#endif
