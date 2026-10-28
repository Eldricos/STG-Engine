/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/combatdata.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace STG {

namespace {

uint32 le32(const byte *p) { return (uint32)p[0] | (uint32)p[1] << 8 | (uint32)p[2] << 16 | (uint32)p[3] << 24; }
uint16 le16(const byte *p) { return (uint16)(p[0] | p[1] << 8); }
uint32 be32(const byte *p) { return (uint32)p[0] << 24 | (uint32)p[1] << 16 | (uint32)p[2] << 8 | p[3]; }
uint16 be16(const byte *p) { return (uint16)(p[0] << 8 | p[1]); }
float f32(const byte *p) {
	uint32 bits = le32(p);
	float f;
	std::memcpy(&f, &bits, 4);
	return f;
}

uint32 argb(byte r, byte g, byte b) { return 0xFF000000u | (uint32)r << 16 | (uint32)g << 8 | b; }

} // End of anonymous namespace

// Models

bool Model3D::load(Common::SeekableReadStream &stream) {
	std::vector<byte> file = stream.readAll();
	parts.clear();
	if (file.size() < 30 + 0x2C || le16(&file[4]) != 110)
		return false;
	const byte *block = &file[30];
	size_t blockSize = file.size() - 30;
	auto ok = [&](uint32 offset, size_t bytes) { return offset <= blockSize && bytes <= blockSize - offset; };

	int count = std::abs((int32)le32(block + 4));
	uint32 records = le32(block + 8);
	for (int i = 0; i < 3; i++) {
		extent[i] = f32(block + 0x10 + 4 * i);
		centre[i] = f32(block + 0x1C + 4 * i);
	}
	radius = std::max(extent[0], std::max(extent[1], extent[2])) / 2;
	if (count > 1000 || !ok(records, (size_t)count * 44))
		return false;
	for (int p = 0; p < count; p++) {
		const byte *r = block + records + 44 * p;
		int verts = std::abs((int32)le32(r)), faces = (int32)le32(r + 4);
		uint32 vertOff = le32(r + 0x0C), uvOff = le32(r + 0x10), faceOff = le32(r + 0x1C), normOff = le32(r + 0x24);
		if (verts > 5000 || faces < 0 || faces > 5000 || !ok(vertOff, (size_t)verts * 12) ||
		    (uvOff && !ok(uvOff, (size_t)verts * 8)) || !ok(faceOff, (size_t)faces * 8) ||
		    (normOff && !ok(normOff, (size_t)faces * 12)))
			return false;
		Part part;
		part.textured = uvOff != 0;
		for (int v = 0; v < verts; v++) {
			Vertex vertex;
			vertex.x = f32(block + vertOff + 12 * v);
			vertex.y = f32(block + vertOff + 12 * v + 4);
			vertex.z = f32(block + vertOff + 12 * v + 8);
			vertex.u = uvOff ? (int32)le32(block + uvOff + 8 * v) / 65536.0f : 0;
			vertex.v = uvOff ? (int32)le32(block + uvOff + 8 * v + 4) / 65536.0f : 0;
			part.vertices.push_back(vertex);
		}
		for (int f = 0; f < faces; f++) {
			const byte *t = block + faceOff + 8 * f;
			Triangle tri;
			tri.material = le16(t);
			tri.a = le16(t + 2);
			tri.b = le16(t + 4);
			tri.c = le16(t + 6);
			if (tri.a >= verts || tri.b >= verts || tri.c >= verts)
				return false;
			tri.nx = normOff ? f32(block + normOff + 12 * f) : 0;
			tri.ny = normOff ? f32(block + normOff + 12 * f + 4) : 1;
			tri.nz = normOff ? f32(block + normOff + 12 * f + 8) : 0;
			part.triangles.push_back(tri);
		}
		parts.push_back(part);
	}
	return true;
}

bool CombatMaterial::loadList(Common::SeekableReadStream &stream, std::vector<CombatMaterial> &list) {
	std::vector<byte> file = stream.readAll();
	list.clear();
	if (file.size() < 30 || le16(&file[8]) != 110)
		return false;
	uint32 count = le32(&file[4]);
	if (count > 4096 || file.size() < 30 + (size_t)count * 36)
		return false;
	for (uint32 i = 0; i < count; i++) {
		const byte *r = &file[30 + 36 * i];
		CombatMaterial m;
		m.colour = le32(r);
		m.param = le32(r + 4);
		m.flags = le32(r + 8);
		for (int c = 0; c < 20 && r[16 + c]; c++)
			m.texture += (char)r[16 + c];
		list.push_back(m);
	}
	return true;
}

uint32 CombatMaterial::rgb() const {
	return argb((byte)((colour >> 10 & 31) * 255 / 31), (byte)((colour >> 5 & 31) * 255 / 31),
	            (byte)((colour & 31) * 255 / 31));
}

// Pictures

bool CombatImage::loadGif(Common::SeekableReadStream &stream) {
	std::vector<byte> d = stream.readAll();
	if (d.size() < 13 || std::memcmp(d.data(), "GIF8", 4) != 0)
		return false;
	size_t p = 13;
	uint32 global[256] = {};
	byte flags = d[10];
	if (flags & 0x80) {
		int n = 2 << (flags & 7);
		if (p + 3 * n > d.size())
			return false;
		for (int i = 0; i < n; i++)
			global[i] = argb(d[p + 3 * i], d[p + 3 * i + 1], d[p + 3 * i + 2]);
		p += 3 * n;
	}
	while (p < d.size()) {
		byte block = d[p++];
		if (block == 0x21) {
			// An extension: skip its sub-blocks.
			p++;
			while (p < d.size() && d[p])
				p += d[p] + 1;
			p++;
			continue;
		}
		if (block != 0x2C || p + 9 > d.size())
			return false;
		w = le16(&d[p + 4]);
		h = le16(&d[p + 6]);
		byte imageFlags = d[p + 8];
		p += 9;
		uint32 palette[256];
		std::memcpy(palette, global, sizeof palette);
		if (imageFlags & 0x80) {
			int n = 2 << (imageFlags & 7);
			if (p + 3 * n > d.size())
				return false;
			for (int i = 0; i < n; i++)
				palette[i] = argb(d[p + 3 * i], d[p + 3 * i + 1], d[p + 3 * i + 2]);
			p += 3 * n;
		}
		if (p >= d.size() || w <= 0 || h <= 0)
			return false;
		int minCode = d[p++];
		if (minCode < 2 || minCode > 8)
			return false;
		std::vector<byte> data;
		while (p < d.size() && d[p]) {
			size_t n = d[p];
			if (p + 1 + n > d.size())
				return false;
			data.insert(data.end(), d.begin() + p + 1, d.begin() + p + 1 + n);
			p += n + 1;
		}

		// LZW.
		std::vector<byte> out;
		out.reserve((size_t)w * h);
		int clear = 1 << minCode, end = clear + 1, size = minCode + 1, next = end + 1;
		std::vector<int> prefix(4096, -1);
		std::vector<byte> suffix(4096), first(4096);
		for (int i = 0; i < clear; i++)
			suffix[i] = first[i] = (byte)i;
		int previous = -1;
		uint32 bits = 0;
		int count = 0;
		std::vector<byte> stack;
		for (size_t i = 0; i < data.size() && out.size() < (size_t)w * h;) {
			while (count < size && i < data.size()) {
				bits |= (uint32)data[i++] << count;
				count += 8;
			}
			if (count < size)
				break;
			int code = bits & ((1 << size) - 1);
			bits >>= size;
			count -= size;
			if (code == clear) {
				size = minCode + 1;
				next = end + 1;
				previous = -1;
				continue;
			}
			if (code == end)
				break;
			int c = code;
			stack.clear();
			if (code >= next) {
				if (previous < 0)
					break;
				stack.push_back(first[previous]);
				c = previous;
			}
			while (c >= clear) {
				stack.push_back(suffix[c]);
				c = prefix[c];
			}
			stack.push_back((byte)c);
			out.insert(out.end(), stack.rbegin(), stack.rend());
			if (previous >= 0 && next < 4096) {
				prefix[next] = previous;
				suffix[next] = (byte)c;
				first[next] = first[previous];
				next++;
				if (next == (1 << size) && size < 12)
					size++;
			}
			previous = code;
		}
		out.resize((size_t)w * h, 0);

		pixels.resize((size_t)w * h);
		bool interlaced = (imageFlags & 0x40) != 0;
		int row = 0;
		for (int pass = 0; pass < (interlaced ? 4 : 1); pass++) {
			static const int start[4] = { 0, 4, 2, 1 }, step[4] = { 8, 8, 4, 2 };
			for (int y = interlaced ? start[pass] : 0; y < h; y += interlaced ? step[pass] : 1, row++) {
				for (int x = 0; x < w; x++)
					pixels[(size_t)y * w + x] = palette[out[(size_t)row * w + x]];
			}
		}
		return true;
	}
	return false;
}

bool CombatImage::loadLbm(Common::SeekableReadStream &stream) {
	std::vector<byte> d = stream.readAll();
	if (d.size() < 12 || std::memcmp(d.data(), "FORM", 4) != 0 || std::memcmp(&d[8], "PBM ", 4) != 0)
		return false;
	uint32 palette[256] = {};
	int compression = 0;
	for (size_t p = 12; p + 8 <= d.size();) {
		uint32 size = be32(&d[p + 4]);
		const byte *c = &d[p + 8];
		if (p + 8 + size > d.size())
			return false;
		if (!std::memcmp(&d[p], "BMHD", 4) && size >= 20) {
			w = be16(c);
			h = be16(c + 2);
			compression = c[10];
		} else if (!std::memcmp(&d[p], "CMAP", 4)) {
			for (uint32 i = 0; i < size / 3 && i < 256; i++)
				palette[i] = argb(c[3 * i], c[3 * i + 1], c[3 * i + 2]);
		} else if (!std::memcmp(&d[p], "BODY", 4)) {
			std::vector<byte> raw;
			if (compression == 0) {
				raw.assign(c, c + size);
			} else {
				for (uint32 i = 0; i < size;) {
					int n = c[i++];
					if (n < 128) {
						for (int k = 0; k <= n && i < size; k++)
							raw.push_back(c[i++]);
					} else if (n > 128 && i < size) {
						raw.insert(raw.end(), 257 - n, c[i++]);
					}
				}
			}
			int stride = (w + 1) & ~1;
			pixels.assign((size_t)w * h, 0);
			for (int y = 0; y < h; y++) {
				for (int x = 0; x < w; x++) {
					size_t i = (size_t)y * stride + x;
					pixels[(size_t)y * w + x] = i < raw.size() ? palette[raw[i]] : 0;
				}
			}
			return w > 0 && h > 0;
		}
		p += 8 + size + (size & 1);
	}
	return false;
}

bool CombatImage::loadRas(Common::SeekableReadStream &stream) {
	std::vector<byte> d = stream.readAll();
	if (d.size() < 32 || be32(d.data()) != 0x59A66A95)
		return false;
	w = (int)be32(&d[4]);
	h = (int)be32(&d[8]);
	uint32 depth = be32(&d[12]), mapLength = be32(&d[28]);
	if (depth != 8 || w <= 0 || h <= 0 || w > 4096 || h > 4096 || 32 + mapLength > d.size())
		return false;
	uint32 n = mapLength / 3;
	uint32 palette[256] = {};
	for (uint32 i = 0; i < n && i < 256; i++) {
		byte r = d[32 + i], g = d[32 + n + i], b = d[32 + 2 * n + i];
		palette[i] = r == 255 && g == 0 && b == 163 ? 0 : argb(r, g, b);
	}
	size_t stride = ((size_t)w + 1) & ~(size_t)1;
	const byte *rows = &d[32 + mapLength];
	size_t available = d.size() - 32 - mapLength;
	pixels.assign((size_t)w * h, 0);
	indices.assign((size_t)w * h, 0);
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			size_t i = y * stride + x;
			if (i < available) {
				pixels[(size_t)y * w + x] = palette[rows[i]];
				indices[(size_t)y * w + x] = rows[i];
			}
		}
	}
	return true;
}

bool loadStandardPalette(Common::SeekableReadStream &stream, uint32 palette[256]) {
	byte data[768];
	if (stream.read(data, 768) != 768)
		return false;
	for (int i = 0; i < 256; i++)
		palette[i] = argb((byte)(data[3 * i] * 255 / 63), (byte)(data[3 * i + 1] * 255 / 63),
		                  (byte)(data[3 * i + 2] * 255 / 63));
	return true;
}

// Animations

namespace {

/** The SPR frames' bit reader: MSB first, 32 bits peeked at a time. */
class Bits {
public:
	Bits(const std::vector<byte> &d, size_t p) : _d(d), _p(p) {}
	uint32 peek() {
		while (_n < 25) {
			byte b = _p < _d.size() ? _d[_p] : 0;
			_acc |= (uint32)b << (24 - _n);
			_p++;
			_n += 8;
		}
		return _acc;
	}
	void drop(int k) {
		_acc <<= k;
		_n -= k;
	}

private:
	const std::vector<byte> &_d;
	size_t _p;
	uint32 _acc = 0;
	int _n = 0;
};

void decodeSprFrame(const std::vector<byte> &d, size_t q, int w, int h, int mode, std::vector<byte> &out) {
	size_t total = (size_t)w * h;
	byte bg = d[q];
	out.clear();
	if (mode == 4) {
		for (size_t i = 0; i < total; i++) {
			byte v = q + 4 + i < d.size() ? d[q + 4 + i] : bg;
			out.push_back(v == bg ? 0 : v);
		}
		return;
	}
	Bits bits(d, q + 4);
	auto emit = [&](int value, int count) {
		for (int i = 0; i < count && out.size() < total; i++)
			out.push_back((byte)value);
	};
	if (mode == 2) {
		while (out.size() < total) {
			uint32 x = bits.peek();
			switch (x >> 30) {
			case 0:
				emit(bg, (int)((x >> 22) & 0xFF) + 1);
				bits.drop(10);
				break;
			case 1:
				emit((x >> 22) & 0xFF, 1);
				bits.drop(10);
				break;
			case 2:
				emit((x >> 22) & 0xFF, (int)((x >> 19) & 7) + 1);
				bits.drop(13);
				break;
			default:
				emit((x >> 22) & 0xFF, (int)((x >> 14) & 0xFF) + 1);
				bits.drop(18);
				break;
			}
		}
	} else {
		// Mode 1: each value predicted from the last (0x498050).
		int prev = -1;
		while (out.size() < total) {
			uint32 x = bits.peek(), t = x >> 26;
			if (t >= 0x20 && t <= 0x2F) {
				int n = (int)((x >> 25) & 7) + 1;
				int u = (int)((x >> 28) & 3);
				if (u & 2)
					u = -u;
				prev += u + 1;
				emit(prev & 0xFF, n);
				bits.drop(7);
			} else if (t >= 0x30 && t <= 0x37) {
				int n = (int)((x >> 21) & 1) + 1;
				prev = (int)((x >> 22) & 0x7F) + 0x80;
				emit(prev, n);
				bits.drop(11);
			} else if (t >= 0x38 && t <= 0x3B) {
				emit(bg, (int)((x >> 23) & 0x1F) + 1);
				prev = bg;
				bits.drop(9);
			} else if (t == 0x3C || t == 0x3D) {
				prev = (int)((x >> 20) & 0x7F) + 0x80;
				emit(prev, (int)((x >> 16) & 0xF) + 1);
				bits.drop(16);
			} else if (t == 0x3E) {
				prev = (int)((x >> 18) & 0xFF);
				emit(prev, (int)((x >> 10) & 0xFF) + 1);
				bits.drop(22);
			} else if (t == 0x3F) {
				emit(bg, w);
				prev = bg;
				bits.drop(7);
			} else {
				int u = (int)(x >> 28);
				prev += (u & 4) == 0 ? (u & 7) + 1 : -1 - (u & 3);
				emit(prev & 0xFF, 1);
				bits.drop(4);
			}
		}
	}
	// The background is see-through.
	for (byte &v : out) {
		if (v == bg)
			v = 0;
	}
}

} // End of anonymous namespace

bool SprAnimation::load(Common::SeekableReadStream &stream) {
	std::vector<byte> d = stream.readAll();
	frames.clear();
	if (d.size() < 12 || std::memcmp(d.data(), "TRPS", 4) != 0)
		return false;
	struct Frame {
		int w, h;
		size_t data;
	};
	std::vector<Frame> found;
	for (size_t p = 12; p + 8 <= d.size();) {
		char id[5] = { (char)d[p + 3], (char)d[p + 2], (char)d[p + 1], (char)d[p], 0 };
		uint32 size = le32(&d[p + 4]);
		if (!std::strcmp(id, "LIST")) {
			p += 16;
			continue;
		}
		if (!std::strcmp(id, "TIME") && p + 12 <= d.size())
			frameMillis = std::max<uint32>(20, le32(&d[p + 8]));
		if (!std::strcmp(id, "COMP") && p + 20 <= d.size())
			found.push_back({ (int)le32(&d[p + 8]), (int)le32(&d[p + 12]), p + 16 });
		if (size == 0)
			break;
		p += size;
	}
	if (found.empty())
		return false;
	w = found[0].w;
	h = found[0].h;
	for (const Frame &f : found) {
		if (f.w != w || f.h != h || f.w <= 0 || f.h <= 0 || (size_t)w * h > 1 << 22 || f.data + 4 > d.size())
			return false;
		size_t q = f.data;
		int mode = d[q + 2];
		if (mode == 3) {
			// The same as an earlier frame.
			if (q + 8 > d.size())
				return false;
			int32 back = (int32)le32(&d[q + 4]);
			if (back <= 0 || (size_t)back > q)
				return false;
			q -= back;
			mode = d[q + 2];
		}
		std::vector<byte> frame;
		decodeSprFrame(d, q, w, h, mode, frame);
		frames.push_back(frame);
	}
	// The 'TIME' chunk is ms a frame... but 666 is far too slow for a
	// beam; the code steps frames on its own clock. Keep a lively rate.
	frameMillis = std::min<uint32>(frameMillis, 90);
	return true;
}

bool MetFont::load(Common::SeekableReadStream &stream) {
	byte data[96 * 8];
	if (stream.read(data, sizeof data) != sizeof data)
		return false;
	for (int i = 0; i < 96; i++)
		cells[i] = { be16(data + 8 * i), be16(data + 8 * i + 2), be16(data + 8 * i + 4), be16(data + 8 * i + 6) };
	return true;
}

// SYSTEMS.DAT and the missions

namespace {

/** Lines without comments (#, $ and C-style), split into words. */
std::vector<std::vector<Common::String>> words(const Common::String &text) {
	std::vector<std::vector<Common::String>> lines;
	for (Common::String line : Common::split(text, '\n')) {
		size_t comment = line.find("/*");
		if (comment != Common::String::npos)
			line.resize(comment);
		line = Common::trim(line);
		if (line.empty() || line[0] == '#' || line[0] == '$')
			continue;
		std::vector<Common::String> w;
		size_t i = 0;
		while (i < line.size()) {
			while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r'))
				i++;
			size_t start = i;
			while (i < line.size() && line[i] != ' ' && line[i] != '\t' && line[i] != '\r')
				i++;
			if (i > start)
				w.push_back(line.substr(start, i - start));
		}
		if (!w.empty())
			lines.push_back(w);
	}
	return lines;
}

bool isNumber(const Common::String &s) {
	return !s.empty() && (std::isdigit((byte)s[0]) || ((s[0] == '-' || s[0] == '+') && s.size() > 1));
}

} // End of anonymous namespace

bool ShipClass::loadAll(const Common::String &text, std::vector<ShipClass> &classes) {
	classes.clear();
	std::vector<std::vector<Common::String>> lines = words(text);
	for (size_t i = 0; i < lines.size(); i++) {
		const std::vector<Common::String> &head = lines[i];
		if (head.size() < 10 || isNumber(head[0]))
			continue;
		ShipClass c;
		c.token = Common::toLower(head[0]);
		c.maxSpeed = std::atoi(head[1].c_str());
		c.agility = std::atoi(head[2].c_str());
		c.hitPoints = std::atoi(head[3].c_str());
		c.closeRadius = std::atoi(head[4].c_str());
		c.destruct = std::atoi(head[5].c_str());
		c.phasers = std::atoi(head[6].c_str());
		c.photons = std::atoi(head[7].c_str());
		c.shields = std::max(0, std::min(4, std::atoi(head[8].c_str())));
		c.torpedoes = std::atoi(head[9].c_str());
		size_t j = i + 1;
		for (int s = 0; s < kSystemCount && j < lines.size(); s++, j++) {
			const std::vector<Common::String> &l = lines[j];
			if (l.size() < 4 || !isNumber(l[0]))
				return false;
			SystemInfo &info = c.systems[s];
			info.max = std::atoi(l[0].c_str());
			info.charge = std::atoi(l[1].c_str());
			info.discharge = std::atoi(l[2].c_str());
			info.repair = std::atoi(l[3].c_str());
			if (s >= kPhaser0) {
				// The arcs are a string of octant digits.
				if (l.size() > 4)
					for (char ch : l[4])
						if (ch >= '0' && ch <= '7')
							info.arcs |= 1 << (ch - '0');
			} else if (l.size() > 5) {
				info.threshold = std::atoi(l[4].c_str());
				info.aimed = std::atoi(l[5].c_str());
			}
		}
		// The shields' leak tables, one line of 13 per shield the ship has.
		int shield = 0;
		for (int s = 1; s <= 4; s++) {
			if (c.systems[s].max < 0 || j >= lines.size() || lines[j].size() != 13)
				continue;
			for (int k = 0; k < 13; k++)
				c.bleed[s][k] = std::atoi(lines[j][k].c_str());
			j++;
			shield++;
		}
		if (j < lines.size() && lines[j].size() >= 3) {
			c.phaserRange = std::atoi(lines[j][1].c_str());
			j++;
		}
		if (j < lines.size() && lines[j].size() >= 3) {
			c.photonRange = std::atoi(lines[j][1].c_str());
			c.photonDamage = std::atoi(lines[j][2].c_str());
			j++;
		}
		if (j < lines.size() && lines[j].size() >= 10) {
			for (int k = 0; k < 10; k++)
				c.cone[k] = std::atoi(lines[j][k].c_str());
			j++;
		}
		classes.push_back(c);
		i = j - 1;
	}
	return !classes.empty();
}

bool loadCombatMission(const Common::String &text, std::vector<MissionShip> &ships) {
	ships.clear();
	int wave = 1;
	for (Common::String line : Common::split(text, '\n')) {
		line = Common::trim(line);
		if (line.empty() || line[0] == '#' || line[0] == '$')
			continue;
		if (line.compare(0, 5, ":wave") == 0) {
			wave = std::atoi(line.c_str() + 5);
			continue;
		}
		if (line[0] == ':')
			continue;
		std::vector<std::vector<Common::String>> w = words(line);
		if (w.empty() || w[0].size() < 9 || !isNumber(w[0][0]))
			continue;
		const std::vector<Common::String> &f = w[0];
		MissionShip ship;
		ship.type = Common::toLower(f[1]);
		ship.x = std::atoi(f[2].c_str());
		ship.y = std::atoi(f[3].c_str());
		ship.z = std::atoi(f[4].c_str());
		ship.yaw = std::atoi(f[5].c_str());
		ship.pitch = std::atoi(f[6].c_str());
		ship.ai = Common::toLower(f[8]);
		ship.wave = wave;
		ships.push_back(ship);
	}
	return !ships.empty();
}

} // End of namespace STG
