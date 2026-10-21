/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_SPRITE_H
#define STG_SPRITE_H

#include <vector>

#include "common/graphics/surface.h"
#include "common/stream.h"

namespace STG {

/**
 * A set of billboard sprites for the 3D away missions (.GS2), e.g. a
 * crew member walking (verid1/001.gs2) or a flame (008.gs2).
 *
 * Layout, worked out from verid1/001.gs2 and 008.gs2:
 *
 *   uint32   2 (version; .GSF files have 1 and a different layout)
 *   uint32   frame count
 *   uint32   0
 *   uint32   sequence count (16 in both samples)
 *   uint32   0
 *   uint16   ?, uint16 ?  (64, 128 / 128, 256 ... a size hint)
 *   sequences: uint8 n, then n frame indices
 *   frames:
 *     uint32 half width at scale 1.0, 16.16 (0.609 for a 78 column frame)
 *     uint32 column count
 *     uint32 column height
 *     uint8  pixels[columns][height]   column-major, bottom row first,
 *                                      0xFF is transparent
 *     uint8  tables[3][columns]        per-column trimming hints (the
 *                                      first is twice the topmost opaque
 *                                      row); not needed to draw
 *
 * Both samples parse to exactly their end with the advertised frame count.
 */
class SpriteSet {
public:
	static const byte kTransparent = 0xFF;

	struct Frame {
		/** Upright image; transparent pixels hold kTransparent. */
		Graphics::Surface surface;
		/**
		 * Half the width in world units for a sprite at scale 1.0, from the
		 * header's 16.16 value. Drawn actors scale it (0x4C3C10).
		 */
		double halfWidth = 0;
	};

	bool load(Common::SeekableReadStream &stream);

	int getFrameCount() const { return (int)_frames.size(); }
	const Frame &getFrame(int index) const { return _frames[index]; }
	const std::vector<std::vector<byte>> &getSequences() const { return _sequences; }

	/** Largest frame size, for laying frames out on screen. */
	int getMaxWidth() const;
	int getMaxHeight() const;

private:
	std::vector<Frame> _frames;
	std::vector<std::vector<byte>> _sequences;
};

} // End of namespace STG

#endif
