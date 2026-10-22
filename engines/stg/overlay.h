/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_OVERLAY_H
#define STG_OVERLAY_H

#include <vector>

#include "common/graphics/surface.h"
#include "common/stream.h"

namespace STG {

/**
 * A first-person overlay animation (.OVR), e.g. Kirk's fist in
 * KIRKHIT.OVR or the hand holding a plant sample in NUPLANT.OVR.
 *
 * Layout, worked out from NUPLANT.OVR and KIRKHIT.OVR:
 *
 *   char[8]  "OVERLAY\0"
 *   uint32   width
 *   uint32   total rows (all frames stacked vertically)
 *   uint32   offset of each row, relative to the end of this table
 *   rows, each a sequence of int16 values until the row is `width` wide:
 *     negative n: skip -n transparent pixels
 *     positive n: n palette indices follow, padded to an even length
 *
 * Every row of both samples decodes to exactly the width. The file does
 * not say how tall a frame is; presumably the game script does. Until
 * that is known, guessFrameHeight() makes an educated guess.
 */
class Overlay {
public:
	bool load(Common::SeekableReadStream &stream);

	int getWidth() const { return _surface.w; }
	int getRowCount() const { return _surface.h; }

	/** All rows decoded; transparent pixels hold 0 and are flagged in the mask. */
	const Graphics::Surface &getSurface() const { return _surface; }
	bool isOpaque(int x, int y) const { return _mask[y * _surface.w + x] != 0; }

	/**
	 * Copy one frame of the given height into dst at (x, y), leaving
	 * transparent pixels untouched. Frames are stacked top down, but each
	 * one's rows are stored bottom up: the plant sampler's nozzle lands on
	 * the hotspots sgens.exe gives its poses only that way up.
	 */
	void drawFrame(Graphics::Surface &dst, int frame, int frameHeight, int x, int y) const;

	/**
	 * Guess the frame height. 252 rows divides every overlay seen so far
	 * that is 504 or 640 wide; otherwise pick the divisor of the row count
	 * that makes consecutive frames most alike.
	 */
	int guessFrameHeight() const;

private:
	Graphics::Surface _surface;
	std::vector<byte> _mask;
};

} // End of namespace STG

#endif
