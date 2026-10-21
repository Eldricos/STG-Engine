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
#include <vector>

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
 * them belong here as they are reverse engineered. Images are 8-bit BMPs,
 * speech and music are PCM WAVs, and each speech sample has a .TXT
 * subtitle beside it.
 */
/** A resource inside a data archive, or a loose file if archive is empty. */
struct ResourceRef {
	Common::String archive;
	Common::String name;

	Common::String toString() const { return archive.empty() ? name : archive + ":" + name; }
};

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

	/** Index every image and sound, in the archives and as loose files. */
	void scan();

	const std::vector<ResourceRef> &getImages() const { return _images; }
	const std::vector<ResourceRef> &getSounds() const { return _sounds; }

	/** Log a summary of each archive's contents by file type. */
	void logInventory() const;

	bool hasResource(const ResourceRef &ref) const;
	std::unique_ptr<Common::SeekableReadStream> open(const ResourceRef &ref) const;

	bool loadImage(const ResourceRef &ref, Image::BitmapDecoder &decoder) const;

	/**
	 * The palette of the first 8-bit image in an archive, for resources
	 * that have none of their own (such as overlays).
	 */
	bool loadArchivePalette(const Common::String &archive, Graphics::Palette &palette) const;
	std::unique_ptr<Audio::RewindableAudioStream> loadSound(const ResourceRef &ref) const;

	/**
	 * The subtitle for a speech sample: the text of the .TXT file next to
	 * it (e.g. SOUNDS/RIKE0107.TXT for SOUNDS/RIKE0107.WAV), or empty.
	 */
	Common::String loadSubtitle(const ResourceRef &soundRef) const;

private:
	/** Installed DATA folder, then each disc's GENS folder. */
	std::vector<Common::FSNode> _dataDirs;
	std::map<Common::String, std::shared_ptr<Common::ZipArchive>> _archives;
	std::vector<ResourceRef> _images;
	std::vector<ResourceRef> _sounds;
};

} // End of namespace STG

#endif
