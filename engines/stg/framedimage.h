/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_FRAMEDIMAGE_H
#define STG_FRAMEDIMAGE_H

#include "common/graphics/palette.h"
#include "common/graphics/surface.h"
#include "common/rect.h"

namespace Image {
class BitmapDecoder;
}

namespace STG {

/**
 * One of the game's interface bitmaps: a strip of frames of frameHeight
 * rows (0 for one frame), drawn in the screen's palette.
 */
struct FramedImage {
	/** The bitmap's own indices; keys are compared with these. */
	Graphics::Surface surface;
	/** Its colours in the screen's palette (Windows matched a DIB's colours to the screen's). */
	byte remap[256];
	/** The index of the first pixel in the file: the bottom left. */
	byte firstPixel = 0;
	int frameHeight = 0;

	enum Key {
		kOpaque = -1,
		/**
		 * See-through where a pixel matches the first one in the file, its
		 * bottom left (0x425A00): the inventory icons' purple, the power
		 * arrow's red, the health bar's magenta.
		 */
		kFirstPixel = -2
	};

	/** Take a decoded bitmap; a negative frame height asks for that many frames. */
	void set(const ::Image::BitmapDecoder &decoder, const Graphics::Palette &palette, int frameHeight);

	int getFrameCount() const;
	int getHeight() const { return frameHeight > 0 ? frameHeight : surface.h; }

	/**
	 * Copy a frame to (x, y), skipping pixels equal to key (an index, or a
	 * Key). Frames count up from the bottom of the strip.
	 */
	void draw(Graphics::Surface &dst, int frame, int x, int y, int key = kOpaque,
	          const Common::Rect *clip = nullptr) const;
};

} // End of namespace STG

#endif
