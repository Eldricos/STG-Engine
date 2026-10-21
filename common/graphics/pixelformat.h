/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_GRAPHICS_PIXELFORMAT_H
#define COMMON_GRAPHICS_PIXELFORMAT_H

#include "common/str.h"
#include "common/types.h"

namespace Graphics {

/**
 * Describes the layout of a pixel. bytesPerPixel == 1 is a palettized
 * (CLUT8) format; otherwise the loss/shift pairs describe each channel.
 */
struct PixelFormat {
	byte bytesPerPixel = 1;
	byte rLoss = 8, gLoss = 8, bLoss = 8, aLoss = 8;
	byte rShift = 0, gShift = 0, bShift = 0, aShift = 0;

	PixelFormat() {}
	PixelFormat(byte bpp, byte rBits, byte gBits, byte bBits, byte aBits,
	            byte rSh, byte gSh, byte bSh, byte aSh)
		: bytesPerPixel(bpp), rLoss(8 - rBits), gLoss(8 - gBits), bLoss(8 - bBits), aLoss(8 - aBits),
		  rShift(rSh), gShift(gSh), bShift(bSh), aShift(aSh) {}

	static PixelFormat createFormatCLUT8() { return PixelFormat(); }
	/** 16-bit 5-6-5, the usual high-colour mode of the era. */
	static PixelFormat createFormatRGB565() { return PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0); }
	/** 16-bit 5-5-5, used by many Windows 95 DirectDraw titles. */
	static PixelFormat createFormatRGB555() { return PixelFormat(2, 5, 5, 5, 0, 10, 5, 0, 0); }
	/** 32-bit 0xAARRGGBB in native endianness. */
	static PixelFormat createFormatARGB8888() { return PixelFormat(4, 8, 8, 8, 8, 16, 8, 0, 24); }

	bool isCLUT8() const { return bytesPerPixel == 1; }

	bool operator==(const PixelFormat &f) const {
		return bytesPerPixel == f.bytesPerPixel && rLoss == f.rLoss && gLoss == f.gLoss && bLoss == f.bLoss &&
		       aLoss == f.aLoss && rShift == f.rShift && gShift == f.gShift && bShift == f.bShift && aShift == f.aShift;
	}
	bool operator!=(const PixelFormat &f) const { return !(*this == f); }

	uint32 RGBToColor(byte r, byte g, byte b) const {
		return ARGBToColor(0xFF, r, g, b);
	}

	uint32 ARGBToColor(byte a, byte r, byte g, byte b) const {
		return ((uint32)(a >> aLoss) << aShift) | ((uint32)(r >> rLoss) << rShift) |
		       ((uint32)(g >> gLoss) << gShift) | ((uint32)(b >> bLoss) << bShift);
	}

	void colorToARGB(uint32 color, byte &a, byte &r, byte &g, byte &b) const {
		a = aLoss >= 8 ? 0xFF : expand(color >> aShift, aLoss);
		r = expand(color >> rShift, rLoss);
		g = expand(color >> gShift, gLoss);
		b = expand(color >> bShift, bLoss);
	}

	void colorToRGB(uint32 color, byte &r, byte &g, byte &b) const {
		byte a;
		colorToARGB(color, a, r, g, b);
	}

	Common::String toString() const;

private:
	/** Scale a channel back up to 8 bits, replicating the high bits into the low ones. */
	static byte expand(uint32 value, byte loss) {
		if (loss >= 8)
			return 0;
		uint32 bits = 8 - loss;
		value &= (1u << bits) - 1;
		uint32 v = value << loss;
		return (byte)(v | (v >> bits));
	}
};

} // End of namespace Graphics

#endif
