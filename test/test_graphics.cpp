/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "test/test.h"

#include "common/graphics/palette.h"
#include "common/graphics/surface.h"
#include "common/image/bmp.h"
#include "common/stream.h"

TEST(pixelformat_conversion) {
	Graphics::PixelFormat f565 = Graphics::PixelFormat::createFormatRGB565();
	uint32 c = f565.RGBToColor(255, 0, 255);
	CHECK_EQ(c, 0xF81Fu);
	byte r, g, b;
	f565.colorToRGB(c, r, g, b);
	CHECK_EQ(r, 255);
	CHECK_EQ(g, 0);
	CHECK_EQ(b, 255);

	Graphics::PixelFormat argb = Graphics::PixelFormat::createFormatARGB8888();
	CHECK_EQ(argb.RGBToColor(0x12, 0x34, 0x56), 0xFF123456u);
	CHECK(Graphics::PixelFormat::createFormatCLUT8().isCLUT8());
	CHECK(argb != f565);
}

TEST(surface_drawing) {
	Graphics::Surface s;
	s.create(8, 4, Graphics::PixelFormat::createFormatCLUT8());
	s.fillRect(Common::Rect(-2, -2, 3, 2), 5);
	CHECK_EQ(s.getPixel(0, 0), 5u);
	CHECK_EQ(s.getPixel(2, 1), 5u);
	CHECK_EQ(s.getPixel(3, 1), 0u);
	CHECK_EQ(s.getPixel(0, 2), 0u);

	s.drawLine(0, 3, 7, 3, 9);
	CHECK_EQ(s.getPixel(7, 3), 9u);
	s.setPixel(100, 100, 1); // clipped, must not crash

	const byte block[4] = { 1, 2, 3, 4 };
	s.copyRectToSurface(block, 2, 7, -1, 2, 2); // mostly off-screen
	CHECK_EQ(s.getPixel(7, 0), 3u);

	Graphics::Surface moved(std::move(s));
	CHECK(s.empty());
	CHECK_EQ(moved.getPixel(7, 0), 3u);
}

TEST(palette_helpers) {
	Graphics::Palette pal;
	pal.set(1, 255, 0, 0);
	pal.set(2, 0, 0, 250);
	CHECK_EQ(pal.findBestColor(0, 10, 240), 2);
	CHECK_EQ(pal.findBestColor(200, 20, 20), 1);

	const byte vga[3] = { 63, 0, 32 };
	pal.setFromVGA(vga, 3, 1);
	byte r, g, b;
	pal.get(3, r, g, b);
	CHECK_EQ(r, 255);
	CHECK_EQ(g, 0);
	CHECK_EQ(b, 130);
}

std::vector<byte> Test::makeBMP8(int width, int height, const std::vector<byte> &pixels, bool rle) {
	Common::MemoryWriteStreamDynamic w;
	int pitch = (width + 3) & ~3;
	std::vector<byte> data;
	if (rle) {
		data = pixels;
	} else {
		// Bottom-up rows, padded to 4 bytes.
		for (int y = height - 1; y >= 0; y--) {
			for (int x = 0; x < pitch; x++)
				data.push_back(x < width ? pixels[y * width + x] : 0);
		}
	}
	uint32 offset = 14 + 40 + 4 * 2;
	w.writeByte('B');
	w.writeByte('M');
	w.writeUint32LE(offset + (uint32)data.size());
	w.writeUint32LE(0);
	w.writeUint32LE(offset);
	w.writeUint32LE(40);
	w.writeUint32LE(width);
	w.writeUint32LE(height);
	w.writeUint16LE(1);
	w.writeUint16LE(8);
	w.writeUint32LE(rle ? 1 : 0);
	w.writeUint32LE((uint32)data.size());
	w.writeUint32LE(0);
	w.writeUint32LE(0);
	w.writeUint32LE(2); // colours used
	w.writeUint32LE(0);
	// Palette, BGRx
	w.writeUint32LE(0x00000000);
	w.writeUint32LE(0x00FF8000); // r=255 g=128 b=0
	w.write(data.data(), (uint32)data.size());
	return w.getData();
}

TEST(bmp_uncompressed_8bit) {
	std::vector<byte> pixels = { 0, 1, 1, 1, 0, 0 }; // 3x2, top row first
	std::vector<byte> file = Test::makeBMP8(3, 2, pixels, false);
	Common::MemoryReadStream s(file.data(), (uint32)file.size());
	Image::BitmapDecoder decoder;
	CHECK(decoder.loadStream(s));
	const Graphics::Surface &surf = decoder.getSurface();
	CHECK_EQ(surf.w, 3);
	CHECK_EQ(surf.h, 2);
	CHECK(decoder.hasPalette());
	CHECK_EQ(decoder.getPaletteColorCount(), 2);
	CHECK_EQ(surf.getPixel(1, 0), 1u);
	CHECK_EQ(surf.getPixel(0, 1), 1u);
	CHECK_EQ(surf.getPixel(2, 1), 0u);
	byte r, g, b;
	decoder.getPalette().get(1, r, g, b);
	CHECK_EQ(r, 255);
	CHECK_EQ(g, 128);
	CHECK_EQ(b, 0);
}

TEST(bmp_rle8) {
	// Row 0 (bottom): run of 4 x colour 1. Row 1: absolute run 1,0,1 + pad.
	std::vector<byte> rle = { 4, 1, 0, 0, 0, 3, 1, 0, 1, 0, 0, 1 };
	std::vector<byte> file = Test::makeBMP8(4, 2, rle, true);
	Common::MemoryReadStream s(file.data(), (uint32)file.size());
	Image::BitmapDecoder decoder;
	CHECK(decoder.loadStream(s));
	const Graphics::Surface &surf = decoder.getSurface();
	CHECK_EQ(surf.getPixel(3, 1), 1u);
	CHECK_EQ(surf.getPixel(0, 0), 1u);
	CHECK_EQ(surf.getPixel(1, 0), 0u);
	CHECK_EQ(surf.getPixel(2, 0), 1u);
	CHECK_EQ(surf.getPixel(3, 0), 0u);
}

TEST(bmp_rejects_garbage) {
	const byte junk[] = "XX not a bitmap";
	Common::MemoryReadStream s(junk, sizeof(junk));
	Image::BitmapDecoder decoder;
	CHECK(!decoder.loadStream(s));
}
