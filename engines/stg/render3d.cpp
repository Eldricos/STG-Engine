/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/render3d.h"

#include <algorithm>
#include <cmath>

namespace STG {

namespace {

const double kFixed = 65536.0;
const double kNear = 0.05;
const double kPi = 3.14159265358979323846;
/** How long each frame of an animated texture shows, in ms. */
const uint32 kFrameTime = 125;

double fromFixed(int32 v) {
	return v / kFixed;
}

} // End of anonymous namespace

struct LevelRenderer::Texture {
	int width = 0;
	int height = 0;
	/** Column-major, as stored: pixels[u * height + v]. */
	std::vector<byte> pixels;
};

struct LevelRenderer::Vertex {
	// Camera space, then screen space after projection.
	double x, y, z;
	double u, v;
};

LevelRenderer::LevelRenderer(const LevelFile &level) : _level(level) {
	_hasSky = _level.loadSky(_sky);
}

LevelRenderer::~LevelRenderer() {}

const LevelRenderer::Texture *LevelRenderer::getTexture(uint16 id, uint16 frames, uint32 time) {
	if (frames > 1)
		id = (uint16)(id + (time / kFrameTime) % frames);

	auto it = _textures.find(id);
	if (it != _textures.end())
		return it->second.get();

	std::unique_ptr<Texture> texture;
	LevelFile::Texture decoded;
	if (_level.loadTexture(id, decoded)) {
		texture.reset(new Texture());
		texture->width = decoded.surface.w;
		texture->height = decoded.surface.h;
		texture->pixels.resize((size_t)texture->width * texture->height);
		for (int u = 0; u < texture->width; u++) {
			for (int v = 0; v < texture->height; v++)
				texture->pixels[(size_t)u * texture->height + v] = (byte)decoded.surface.getPixel(u, v);
		}
	}
	const Texture *result = texture.get();
	_textures[id] = std::move(texture);
	return result;
}

void LevelRenderer::toCamera(double x, double y, double z, Vertex &out) const {
	double dx = x - _camX, dy = y - _camY;
	out.x = dx * _sin - dy * _cos;
	out.z = dx * _cos + dy * _sin;
	out.y = z - _camZ;
}

void LevelRenderer::render(const Camera &camera, Graphics::Surface &dst, uint32 time) {
	_cos = std::cos(camera.yaw);
	_sin = std::sin(camera.yaw);
	_camX = camera.x;
	_camY = camera.y;
	_camZ = camera.z;
	_focal = dst.w / 2.0;
	_centerX = (dst.w - 1) / 2.0;
	_centerY = (dst.h - 1) / 2.0 + camera.pitch * _focal;

	_depth.assign((size_t)dst.w * dst.h, 0.0f);
	drawSky(camera, dst);
	drawFlats(dst, time);
	drawWalls(dst, time);
	drawBillboards(dst, time);
}

void LevelRenderer::drawSky(const Camera &camera, Graphics::Surface &dst) {
	if (!_hasSky) {
		dst.fillRect(Common::Rect(dst.w, dst.h), 0);
		return;
	}
	// One sky pixel per screen pixel, so a 90 degree view covers dst.w
	// columns. The bottom row of the sky sits on the horizon.
	double turn = camera.yaw / (2 * kPi);
	int offset = (int)std::floor(-turn * 4 * dst.w);
	int horizon = (int)std::lround(_centerY);
	for (int y = 0; y < dst.h; y++) {
		int row = std::max(0, std::min<int>(_sky.h - 1, y - (horizon - _sky.h)));
		const byte *src = (const byte *)_sky.getBasePtr(0, row);
		byte *out = (byte *)dst.getBasePtr(0, y);
		for (int x = 0; x < dst.w; x++) {
			int column = (x + offset) % _sky.w;
			if (column < 0)
				column += _sky.w;
			out[x] = src[column];
		}
	}
}

void LevelRenderer::drawFlats(Graphics::Surface &dst, uint32 time) {
	const std::vector<LevelFile::Vertex> &vertices = _level.getVertices();
	std::vector<Vertex> poly;
	for (const LevelFile::Sector &flat : _level.getSectors()) {
		if (!(flat.flags & LevelFile::kFlagVisible))
			continue;
		double height = fromFixed(flat.height);
		// Floors are only seen from above and ceilings from below.
		if (flat.isCeiling() ? height <= _camZ : height >= _camZ)
			continue;

		double uScale = fromFixed(flat.uScale), vScale = fromFixed(flat.vScale);
		double uOffset = fromFixed(flat.uOffset), vOffset = fromFixed(flat.vOffset);
		double c = 1, s = 0;
		if (flat.rotation) {
			double angle = flat.rotation * 2 * kPi / 65536.0;
			c = std::cos(angle);
			s = std::sin(angle);
		}

		poly.clear();
		for (uint16 index : flat.vertices) {
			double x = fromFixed(vertices[index].x), y = fromFixed(vertices[index].y);
			Vertex v;
			toCamera(x, y, height, v);
			v.u = (x * c + y * s) * uScale + uOffset;
			v.v = (y * c - x * s) * vScale + vOffset;
			poly.push_back(v);
		}
		drawPolygon(poly, getTexture(flat.texture, flat.frames, time), dst);
	}
}

void LevelRenderer::drawWalls(Graphics::Surface &dst, uint32 time) {
	const std::vector<LevelFile::Vertex> &vertices = _level.getVertices();
	std::vector<Vertex> poly(4);
	for (const LevelFile::Wall &wall : _level.getWalls()) {
		if (!(wall.flags & LevelFile::kFlagVisible))
			continue;
		double x1 = fromFixed(vertices[wall.vertex1].x), y1 = fromFixed(vertices[wall.vertex1].y);
		double x2 = fromFixed(vertices[wall.vertex2].x), y2 = fromFixed(vertices[wall.vertex2].y);
		double top = fromFixed(wall.top), bottom = fromFixed(wall.bottom);
		double u1 = fromFixed(wall.uStart), u2 = u1 + fromFixed(wall.uLength);
		double v1 = fromFixed(wall.vStart), v2 = v1 + fromFixed(wall.vLength);

		poly.resize(4);
		toCamera(x1, y1, top, poly[0]);
		poly[0].u = u1;
		poly[0].v = v1;
		toCamera(x2, y2, top, poly[1]);
		poly[1].u = u2;
		poly[1].v = v1;
		toCamera(x2, y2, bottom, poly[2]);
		poly[2].u = u2;
		poly[2].v = v2;
		toCamera(x1, y1, bottom, poly[3]);
		poly[3].u = u1;
		poly[3].v = v2;
		drawPolygon(poly, getTexture(wall.texture, wall.frames, time), dst);
	}
}

void LevelRenderer::drawBillboards(Graphics::Surface &dst, uint32 time) {
	std::vector<Vertex> poly(4);
	for (const LevelFile::Billboard &billboard : _level.getBillboards()) {
		const Texture *texture = getTexture(billboard.texture, billboard.frames, time);
		if (!texture)
			continue;
		Vertex centre;
		toCamera(fromFixed(billboard.x), fromFixed(billboard.y), fromFixed(billboard.z), centre);
		double half = fromFixed(billboard.halfWidth), height = fromFixed(billboard.height);

		poly.resize(4);
		const double xs[4] = { -half, half, half, -half };
		const double ys[4] = { height, height, 0, 0 };
		for (int i = 0; i < 4; i++) {
			poly[i].x = centre.x + xs[i];
			poly[i].y = centre.y + ys[i];
			poly[i].z = centre.z;
			poly[i].u = (i == 1 || i == 2) ? texture->width : 0;
			poly[i].v = (i >= 2) ? texture->height : 0;
		}
		drawPolygon(poly, texture, dst);
	}
}

void LevelRenderer::drawPolygon(std::vector<Vertex> &poly, const Texture *texture, Graphics::Surface &dst) {
	// Clip against the near plane.
	std::vector<Vertex> clipped;
	clipped.reserve(poly.size() + 2);
	for (size_t i = 0; i < poly.size(); i++) {
		const Vertex &a = poly[i], &b = poly[(i + 1) % poly.size()];
		bool aIn = a.z >= kNear, bIn = b.z >= kNear;
		if (aIn)
			clipped.push_back(a);
		if (aIn != bIn) {
			double t = (kNear - a.z) / (b.z - a.z);
			Vertex v;
			v.x = a.x + (b.x - a.x) * t;
			v.y = a.y + (b.y - a.y) * t;
			v.z = kNear;
			v.u = a.u + (b.u - a.u) * t;
			v.v = a.v + (b.v - a.v) * t;
			clipped.push_back(v);
		}
	}
	if (clipped.size() < 3)
		return;

	// Project: x and y to the screen; z becomes 1/z and u, v become u/z, v/z
	// so they interpolate linearly across the screen.
	double minY = 1e30, maxY = -1e30, minX = 1e30, maxX = -1e30;
	for (Vertex &v : clipped) {
		double iz = 1.0 / v.z;
		v.x = _centerX + v.x * iz * _focal;
		v.y = _centerY - v.y * iz * _focal;
		v.z = iz;
		v.u *= iz;
		v.v *= iz;
		minY = std::min(minY, v.y);
		maxY = std::max(maxY, v.y);
		minX = std::min(minX, v.x);
		maxX = std::max(maxX, v.x);
	}
	if (maxX < 0 || minX >= dst.w || maxY < 0 || minY >= dst.h)
		return;

	int y0 = std::max(0, (int)std::ceil(minY - 0.5));
	int y1 = std::min<int>(dst.h - 1, (int)std::ceil(maxY - 0.5) - 1);
	size_t n = clipped.size();
	for (int y = y0; y <= y1; y++) {
		double sampleY = y + 0.5;
		// The polygon is convex, so each row crosses it in one span.
		bool found = false;
		Vertex left = {}, right = {};
		for (size_t i = 0; i < n; i++) {
			const Vertex &a = clipped[i], &b = clipped[(i + 1) % n];
			if ((a.y <= sampleY) == (b.y <= sampleY))
				continue;
			double t = (sampleY - a.y) / (b.y - a.y);
			Vertex e;
			e.x = a.x + (b.x - a.x) * t;
			e.z = a.z + (b.z - a.z) * t;
			e.u = a.u + (b.u - a.u) * t;
			e.v = a.v + (b.v - a.v) * t;
			if (!found) {
				left = right = e;
				found = true;
			} else if (e.x < left.x) {
				left = e;
			} else if (e.x > right.x) {
				right = e;
			}
		}
		if (!found || right.x <= left.x)
			continue;

		int x0 = std::max(0, (int)std::ceil(left.x - 0.5));
		int x1 = std::min<int>(dst.w - 1, (int)std::ceil(right.x - 0.5) - 1);
		double width = right.x - left.x;
		double dz = (right.z - left.z) / width, du = (right.u - left.u) / width, dv = (right.v - left.v) / width;
		double start = x0 + 0.5 - left.x;
		double iz = left.z + dz * start, uz = left.u + du * start, vz = left.v + dv * start;
		byte *out = (byte *)dst.getBasePtr(0, y);
		float *depth = &_depth[(size_t)y * dst.w];
		for (int x = x0; x <= x1; x++, iz += dz, uz += du, vz += dv) {
			if (iz <= depth[x])
				continue;
			byte color = 0;
			if (texture) {
				double z = 1.0 / iz;
				int u = (int)std::floor(uz * z) % texture->width;
				int v = (int)std::floor(vz * z) % texture->height;
				if (u < 0)
					u += texture->width;
				if (v < 0)
					v += texture->height;
				color = texture->pixels[(size_t)u * texture->height + v];
				if (color == LevelFile::kTransparent)
					continue;
			}
			out[x] = color;
			depth[x] = (float)iz;
		}
	}
}

namespace {

/** Even-odd point in polygon test on the floor plan. */
bool containsPoint(const std::vector<LevelFile::Vertex> &vertices, const LevelFile::Sector &flat, double x, double y) {
	bool inside = false;
	size_t n = flat.vertices.size();
	for (size_t i = 0, j = n - 1; i < n; j = i++) {
		double xi = fromFixed(vertices[flat.vertices[i]].x), yi = fromFixed(vertices[flat.vertices[i]].y);
		double xj = fromFixed(vertices[flat.vertices[j]].x), yj = fromFixed(vertices[flat.vertices[j]].y);
		if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi)
			inside = !inside;
	}
	return inside;
}

} // End of anonymous namespace

bool LevelRenderer::findFloor(double x, double y, double maxHeight, double &height) const {
	bool found = false;
	for (const LevelFile::Sector &flat : _level.getSectors()) {
		if (flat.isCeiling() || flat.vertices.size() < 3)
			continue;
		double h = fromFixed(flat.height);
		if (h > maxHeight || (found && h <= height))
			continue;
		if (containsPoint(_level.getVertices(), flat, x, y)) {
			height = h;
			found = true;
		}
	}
	return found;
}

Camera LevelRenderer::getStartCamera(double eyeHeight) const {
	Camera camera;
	const std::vector<LevelFile::Vertex> &vertices = _level.getVertices();
	double bestArea = -1;
	for (const LevelFile::Sector &flat : _level.getSectors()) {
		if (flat.isCeiling() || flat.vertices.size() < 3)
			continue;
		double area = 0, cx = 0, cy = 0;
		size_t n = flat.vertices.size();
		for (size_t i = 0; i < n; i++) {
			const LevelFile::Vertex &a = vertices[flat.vertices[i]], &b = vertices[flat.vertices[(i + 1) % n]];
			area += fromFixed(a.x) * fromFixed(b.y) - fromFixed(b.x) * fromFixed(a.y);
			cx += fromFixed(a.x);
			cy += fromFixed(a.y);
		}
		area = std::fabs(area) / 2;
		if (area > bestArea) {
			bestArea = area;
			camera.x = cx / n;
			camera.y = cy / n;
			camera.z = fromFixed(flat.height) + eyeHeight;
		}
	}
	return camera;
}

} // End of namespace STG
