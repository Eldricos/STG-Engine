/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/walker.h"

#include <algorithm>
#include <cmath>

namespace STG {

namespace {

const double kFixed = 65536.0;
/** Distance kept from a wall after touching it. */
const double kSkin = 0.01;

double fromFixed(int32 v) {
	return v / kFixed;
}

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

Walker::Walker(const LevelFile &level) : _level(level) {
	const std::vector<LevelFile::Vertex> &vertices = level.getVertices();
	for (const LevelFile::Wall &wall : level.getWalls()) {
		// Like the original, only walls that are drawn get in the way.
		if (!(wall.flags & LevelFile::kFlagVisible))
			continue;
		WallLine line;
		line.x1 = fromFixed(vertices[wall.vertex1].x);
		line.y1 = fromFixed(vertices[wall.vertex1].y);
		line.x2 = fromFixed(vertices[wall.vertex2].x);
		line.y2 = fromFixed(vertices[wall.vertex2].y);
		line.nx = fromFixed(wall.normalX);
		line.ny = fromFixed(wall.normalY);
		line.d = fromFixed(wall.distance);
		line.top = fromFixed(wall.top);
		line.bottom = fromFixed(wall.bottom);
		line.zones = wall.zones;
		line.bothSides = (wall.flags & LevelFile::kWallSolidBothSides) != 0;
		_walls.push_back(line);
	}
}

void Walker::place(double x, double y, double feet) {
	_x = x;
	_y = y;
	_feet = feet;
	_fallSpeed = 0;
	_zones = kAllZones;
	_floor = findFloor(x, y, feet + kStepHeight, _zones);
	if (_floor >= 0) {
		const LevelFile::Sector &flat = _level.getSectors()[_floor];
		_feet = fromFixed(flat.height);
		_zones = flat.activeZones ? flat.activeZones : kAllZones;
	}
}

void Walker::move(double dx, double dy, double seconds) {
	if (dx != 0 || dy != 0)
		collideAndSlide(dx, dy);
	followFloor(seconds);
}

int Walker::findFloor(double x, double y, double maxHeight, uint64 zones) const {
	int best = -1;
	double bestHeight = 0;
	const std::vector<LevelFile::Sector> &flats = _level.getSectors();
	for (size_t i = 0; i < flats.size(); i++) {
		const LevelFile::Sector &flat = flats[i];
		if (flat.isCeiling() || flat.vertices.size() < 3 || !(flat.zones & zones))
			continue;
		double h = fromFixed(flat.height);
		if (h > maxHeight || (best >= 0 && h <= bestHeight))
			continue;
		if (containsPoint(_level.getVertices(), flat, x, y)) {
			best = (int)i;
			bestHeight = h;
		}
	}
	return best;
}

bool Walker::sweep(double x, double y, double dx, double dy, Hit &hit) const {
	double length2 = dx * dx + dy * dy;
	if (length2 == 0)
		return false;

	hit.t = 2;
	double low = _feet + kStepHeight, high = _feet + kEyeHeight + kHeadroom;
	for (size_t i = 0; i < _walls.size(); i++) {
		const WallLine &w = _walls[i];
		if (!(w.zones & _zones) || w.bottom > high || w.top < low)
			continue;

		// The wall's line, approached from its solid side.
		double nx = w.nx, ny = w.ny, d = w.d;
		double side0 = nx * x + ny * y + d;
		bool facing = true;
		if (side0 < 0) {
			if (w.bothSides) {
				nx = -nx;
				ny = -ny;
				d = -d;
				side0 = -side0;
			} else {
				facing = false;
			}
		}
		if (facing) {
			// Already closer than the radius: let the player move away.
			double r = std::min(kRadius, side0);
			double side1 = nx * (x + dx) + ny * (y + dy) + d;
			if (side1 < r && side0 > side1) {
				double t = std::max(0.0, (side0 - r) / (side0 - side1));
				double qx = x + dx * t - nx * r, qy = y + dy * t - ny * r;
				double wx = w.x2 - w.x1, wy = w.y2 - w.y1;
				double wl2 = wx * wx + wy * wy;
				double s = wl2 > 0 ? ((qx - w.x1) * wx + (qy - w.y1) * wy) / wl2 : -1;
				if (s >= 0 && s <= 1 && t < hit.t) {
					double wl = std::sqrt(wl2);
					hit.t = t;
					hit.slideX = wx / wl;
					hit.slideY = wy / wl;
					hit.wall = (int)i;
				}
			}
		}

		// The wall's ends, as posts the player's circle can catch on.
		const double ends[2][2] = { { w.x1, w.y1 }, { w.x2, w.y2 } };
		for (const auto &end : ends) {
			double px = x - end[0], py = y - end[1];
			double b = 2 * (dx * px + dy * py);
			double c = px * px + py * py - kRadius * kRadius;
			if (b >= 0)
				continue; // moving away
			double t;
			if (c <= 0) {
				t = 0;
			} else {
				double disc = b * b - 4 * length2 * c;
				if (disc < 0)
					continue;
				t = (-b - std::sqrt(disc)) / (2 * length2);
			}
			if (t < 0 || t > 1 || t >= hit.t)
				continue;
			double cx = px + dx * t, cy = py + dy * t;
			double cl = std::sqrt(cx * cx + cy * cy);
			if (cl == 0)
				continue;
			hit.t = t;
			hit.slideX = -cy / cl;
			hit.slideY = cx / cl;
			hit.wall = (int)i;
		}
	}
	return hit.t <= 1;
}

void Walker::collideAndSlide(double dx, double dy) {
	if (!_collision) {
		_x += dx;
		_y += dy;
		return;
	}

	Hit hit;
	if (!sweep(_x, _y, dx, dy, hit)) {
		_x += dx;
		_y += dy;
		return;
	}

	// Stop just short of the wall, then slide along it with the rest.
	double length = std::sqrt(dx * dx + dy * dy);
	double t = std::max(0.0, hit.t - kSkin / length);
	_x += dx * t;
	_y += dy * t;
	double restX = dx * (1 - t), restY = dy * (1 - t);
	double along = restX * hit.slideX + restY * hit.slideY;
	double sx = hit.slideX * along, sy = hit.slideY * along;

	Hit second;
	if (!sweep(_x, _y, sx, sy, second)) {
		_x += sx;
		_y += sy;
		return;
	}
	double slideLength = std::sqrt(sx * sx + sy * sy);
	double t2 = slideLength > 0 ? std::max(0.0, second.t - kSkin / slideLength) : 0;
	_x += sx * t2;
	_y += sy * t2;
}

void Walker::followFloor(double seconds) {
	double maxHeight = _feet + kStepHeight;
	int floor = findFloor(_x, _y, maxHeight, _zones);
	if (floor < 0 && _zones != kAllZones)
		floor = findFloor(_x, _y, maxHeight, kAllZones);
	_floor = floor;
	if (floor < 0) {
		// Nothing underfoot: the original opens up every zone.
		_zones = kAllZones;
		_fallSpeed = 0;
		return;
	}

	const LevelFile::Sector &flat = _level.getSectors()[floor];
	_zones = flat.activeZones ? flat.activeZones : kAllZones;
	double height = fromFixed(flat.height);
	if (height >= _feet) {
		// Step up (or stay on the level).
		_feet = height;
		_fallSpeed = 0;
		return;
	}
	_fallSpeed += kGravity * seconds;
	_feet -= _fallSpeed * seconds;
	if (_feet <= height) {
		_feet = height;
		_fallSpeed = 0;
	}
}

} // End of namespace STG
