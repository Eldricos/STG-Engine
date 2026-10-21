/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_RESOURCES_H
#define STG_RESOURCES_H

#include <map>
#include <memory>

#include "common/audio/audiostream.h"
#include "common/fs.h"
#include "common/image/bmp.h"
#include "common/str.h"
#include "common/zip.h"

namespace STG {

/**
 * Index of the game's resources.
 *
 * The game's data lives in ZIP archives (*.DAT, MUSIC.1/2) in the
 * installed DATA folder and on the CDs. Decoders for the formats inside
 * them belong here as they are reverse engineered. The viewer currently
 * only understands standard Windows formats (BMP, WAV).
 */
class ResourceManager {
public:
	/**
	 * Register the installed DATA directory and the CD data directories
	 * with SearchMan. The 2 CD release keeps COMBAT.DAT, STELLAR.DAT, the
	 * music and the movies on the discs.
	 */
	void addSearchPaths(const Common::FSNode &gameDir, const Common::String &cd1Path, const Common::String &cd2Path);

	/** Warn about any known data file that can't be found. Returns true if all are present. */
	bool checkDataFiles() const;

	/**
	 * Open the game's data archives (the .DAT files and MUSIC.1/2 are
	 * ZIP files). The shared COMMON.DAT and OPTIONS.DAT are added to
	 * SearchMan; per-location archives are opened on demand.
	 */
	void openArchives();

	/** A data archive by file name, e.g. "VERID1.DAT", or nullptr if missing. */
	std::shared_ptr<Common::ZipArchive> getArchive(const Common::String &name) const;

	void scan();

	const Common::StringArray &getImages() const { return _images; }
	const Common::StringArray &getSounds() const { return _sounds; }

	/** Log a summary of what the game directory contains, by extension. */
	void logInventory() const;

	bool loadImage(const Common::String &name, Image::BitmapDecoder &decoder) const;
	std::unique_ptr<Audio::RewindableAudioStream> loadSound(const Common::String &name) const;

private:
	/** Installed DATA folder, then each disc's GENS folder. */
	std::vector<Common::FSNode> _dataDirs;
	std::map<Common::String, std::shared_ptr<Common::ZipArchive>> _archives;
	Common::StringArray _images;
	Common::StringArray _sounds;
};

} // End of namespace STG

#endif
