/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_RECT_H
#define COMMON_RECT_H

#include <algorithm>

#include "common/types.h"

namespace Common {

struct Point {
	int16 x = 0;
	int16 y = 0;

	Point() {}
	Point(int16 x1, int16 y1) : x(x1), y(y1) {}

	bool operator==(const Point &p) const { return x == p.x && y == p.y; }
	bool operator!=(const Point &p) const { return !(*this == p); }
};

/**
 * A rectangle where (left, top) is inclusive and (right, bottom) is exclusive,
 * matching the convention used by ScummVM and by the original Win32 code.
 */
struct Rect {
	int16 top = 0, left = 0;
	int16 bottom = 0, right = 0;

	Rect() {}
	Rect(int16 w, int16 h) : top(0), left(0), bottom(h), right(w) {}
	Rect(int16 x1, int16 y1, int16 x2, int16 y2) : top(y1), left(x1), bottom(y2), right(x2) {}

	int16 width() const { return right - left; }
	int16 height() const { return bottom - top; }
	bool isEmpty() const { return left >= right || top >= bottom; }

	bool contains(int16 x, int16 y) const {
		return left <= x && x < right && top <= y && y < bottom;
	}
	bool contains(const Point &p) const { return contains(p.x, p.y); }

	bool intersects(const Rect &r) const {
		return left < r.right && r.left < right && top < r.bottom && r.top < bottom;
	}

	void clip(const Rect &r) {
		top = std::max(top, r.top);
		left = std::max(left, r.left);
		bottom = std::min(bottom, r.bottom);
		right = std::min(right, r.right);
		if (isEmpty())
			top = left = bottom = right = 0;
	}

	void translate(int16 dx, int16 dy) {
		left += dx;
		right += dx;
		top += dy;
		bottom += dy;
	}
};

} // End of namespace Common

#endif
