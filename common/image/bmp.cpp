/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "common/image/bmp.h"
#include "common/debug.h"

#include <cstdlib>
#include <vector>

namespace Image {

static const uint32 kBI_RGB = 0;
static const uint32 kBI_RLE8 = 1;
static const uint32 kBI_BITFIELDS = 3;

bool BitmapDecoder::loadStream(Common::SeekableReadStream &stream) {
	int64 start = stream.pos();
	if (stream.readByte() != 'B' || stream.readByte() != 'M')
		return false;
	stream.readUint32LE(); // file size
	stream.readUint32LE(); // reserved
	uint32 dataOffset = stream.readUint32LE();
	return loadInternal(stream, start + dataOffset);
}

bool BitmapDecoder::loadDIB(Common::SeekableReadStream &stream) {
	return loadInternal(stream, -1);
}

bool BitmapDecoder::loadInternal(Common::SeekableReadStream &stream, int64 dataOffset) {
	_surface.free();
	_paletteColorCount = 0;

	int64 headerStart = stream.pos();
	uint32 headerSize = stream.readUint32LE();
	if (headerSize < 40) {
		warning("BitmapDecoder: unsupported header size %u (OS/2 bitmaps are not supported)", headerSize);
		return false;
	}

	int32 width = stream.readSint32LE();
	int32 height = stream.readSint32LE();
	stream.readUint16LE(); // planes
	uint16 bitsPerPixel = stream.readUint16LE();
	uint32 compression = stream.readUint32LE();
	stream.readUint32LE(); // image size
	stream.readUint32LE(); // x pixels per metre
	stream.readUint32LE(); // y pixels per metre
	uint32 colorsUsed = stream.readUint32LE();
	stream.readUint32LE(); // important colours

	bool bottomUp = height > 0;
	height = abs(height);
	if (width <= 0 || width > 8192 || height == 0 || height > 8192) {
		warning("BitmapDecoder: invalid dimensions %dx%d", width, height);
		return false;
	}

	uint32 rMask = 0x7C00, gMask = 0x03E0, bMask = 0x001F;
	if (compression == kBI_BITFIELDS) {
		stream.seek(headerStart + 40);
		rMask = stream.readUint32LE();
		gMask = stream.readUint32LE();
		bMask = stream.readUint32LE();
	} else if (compression != kBI_RGB && !(compression == kBI_RLE8 && bitsPerPixel == 8)) {
		warning("BitmapDecoder: unsupported compression %u", compression);
		return false;
	}

	stream.seek(headerStart + headerSize + (compression == kBI_BITFIELDS && headerSize == 40 ? 12 : 0));

	if (bitsPerPixel <= 8) {
		if (bitsPerPixel != 8 && bitsPerPixel != 4 && bitsPerPixel != 1) {
			warning("BitmapDecoder: unsupported bit depth %d", bitsPerPixel);
			return false;
		}
		_paletteColorCount = colorsUsed ? (uint16)colorsUsed : (uint16)(1 << bitsPerPixel);
		if (_paletteColorCount > 256)
			_paletteColorCount = 256;
		for (int i = 0; i < _paletteColorCount; i++) {
			byte b = stream.readByte();
			byte g = stream.readByte();
			byte r = stream.readByte();
			stream.readByte();
			_palette.set(i, r, g, b);
		}
	}

	if (dataOffset >= 0)
		stream.seek(dataOffset);

	if (compression == kBI_RLE8) {
		_surface.create((int16)width, (int16)height, Graphics::PixelFormat::createFormatCLUT8());
		return decodeRLE8(stream, width, height, bottomUp);
	}

	int srcPitch = ((width * bitsPerPixel + 31) / 32) * 4;
	std::vector<byte> row(srcPitch);

	Graphics::PixelFormat format = bitsPerPixel <= 8 ? Graphics::PixelFormat::createFormatCLUT8()
	                                                 : Graphics::PixelFormat::createFormatARGB8888();
	_surface.create((int16)width, (int16)height, format);

	auto maskShift = [](uint32 mask) {
		int shift = 0;
		while (mask && !(mask & 1)) {
			mask >>= 1;
			shift++;
		}
		return shift;
	};
	auto maskBits = [](uint32 mask) {
		int bits = 0;
		for (; mask; mask >>= 1)
			bits += mask & 1;
		return bits;
	};
	auto extract = [&](uint32 pixel, uint32 mask) -> byte {
		if (!mask)
			return 0;
		uint32 v = (pixel & mask) >> maskShift(mask);
		int bits = maskBits(mask);
		if (bits >= 8)
			return (byte)(v >> (bits - 8));
		// Scale up to 8 bits, filling the low bits from the high ones.
		return (byte)(v * 255 / ((1u << bits) - 1));
	};

	for (int y = 0; y < height; y++) {
		if (stream.read(row.data(), srcPitch) != (uint32)srcPitch) {
			warning("BitmapDecoder: unexpected end of pixel data");
			return false;
		}
		int destY = bottomUp ? height - 1 - y : y;

		for (int x = 0; x < width; x++) {
			uint32 color;
			switch (bitsPerPixel) {
			case 1:
				color = (row[x >> 3] >> (7 - (x & 7))) & 1;
				break;
			case 4:
				color = (row[x >> 1] >> ((x & 1) ? 0 : 4)) & 0xF;
				break;
			case 8:
				color = row[x];
				break;
			case 16: {
				uint32 pixel = READ_LE_UINT16(&row[x * 2]);
				color = format.RGBToColor(extract(pixel, rMask), extract(pixel, gMask), extract(pixel, bMask));
				break;
			}
			case 24:
				color = format.RGBToColor(row[x * 3 + 2], row[x * 3 + 1], row[x * 3]);
				break;
			case 32:
				color = format.RGBToColor(row[x * 4 + 2], row[x * 4 + 1], row[x * 4]);
				break;
			default:
				warning("BitmapDecoder: unsupported bit depth %d", bitsPerPixel);
				return false;
			}
			_surface.setPixel(x, destY, color);
		}
	}
	return true;
}

bool BitmapDecoder::decodeRLE8(Common::SeekableReadStream &stream, int width, int height, bool bottomUp) {
	int x = 0, y = 0;
	auto put = [&](byte color) {
		if (x < width && y < height)
			_surface.setPixel(x, bottomUp ? height - 1 - y : y, color);
		x++;
	};

	while (!stream.eos()) {
		byte count = stream.readByte();
		byte value = stream.readByte();
		if (count > 0) {
			for (int i = 0; i < count; i++)
				put(value);
			continue;
		}

		switch (value) {
		case 0: // end of line
			x = 0;
			y++;
			break;
		case 1: // end of bitmap
			return true;
		case 2: // delta
			x += stream.readByte();
			y += stream.readByte();
			break;
		default: { // absolute run, padded to a 16-bit boundary
			for (int i = 0; i < value; i++)
				put(stream.readByte());
			if (value & 1)
				stream.readByte();
			break;
		}
		}
	}
	// Some encoders omit the end of bitmap marker.
	return true;
}

} // End of namespace Image
