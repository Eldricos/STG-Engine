/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_MOVERS_H
#define STG_MOVERS_H

#include <map>
#include <vector>

#include "engines/stg/level.h"

namespace Common {
class Serializer;
}

namespace STG {

class Walker;

/**
 * Doors: walls (and the flats under or over them) that slide, swing or
 * rise between closed and open, as sgens.exe's movers (built by 0x4B7CA0,
 * run by 0x4B82E0).
 *
 * The walls of a mover with no flag 0x2000 are its leaders; the others
 * follow the leader with the same mode. Every step places each part from
 * its original geometry and the mover's position, 0 (closed) to +-0x4000
 * (open one way or the other). A step that would push a part into the
 * player is undone; a closing door that may reverse opens again.
 *
 * States: 0 idle, 1 open, 2 opening, 3 closed, 4 closing, and 5, 6, 7 the
 * same the other way. The mover's script object (0x8000 | id) hears 22
 * when it starts opening, 23 open, 24 starts closing, 25 closed.
 */
class Movers {
public:
	enum Event {
		kEventOpening = 22,
		kEventOpen = 23,
		kEventClosing = 24,
		kEventClosed = 25
	};
	enum State {
		kIdle = 0,
		kOpen = 1,
		kOpening = 2,
		kClosed = 3,
		kClosing = 4,
		kOpenBack = 5,
		kOpeningBack = 6,
		kClosingBack = 7
	};
	static const int kOpenPosition = 0x4000;

	struct Notice {
		uint16 object;
		uint16 event;
	};

	explicit Movers(LevelFile &level);

	int getCount() const { return (int)_movers.size(); }

	/**
	 * Run every moving mover by ms (clamped like the engine's tick),
	 * keeping clear of the player. Walls that moved are added to moved.
	 */
	void update(uint32 ms, Walker &player, std::vector<int> &moved, std::vector<Notice> &notices);

	/**
	 * The natives (0x39..0x3F) set the state at once (with no lock check):
	 * opening, closing, open, closed. Open and closed also move the door.
	 */
	void setState(uint16 id, int state, Walker &player, std::vector<int> &moved, std::vector<Notice> &notices);
	/** 0x3B: open a closed door, close an open one. */
	void toggle(uint16 id, Walker &player, std::vector<int> &moved, std::vector<Notice> &notices);
	/** 0x3C / 0x3D: a locked door doesn't open when walked into. */
	void setLocked(uint16 id, bool locked);
	/** The player walked into a wall (0x4B94D0): it may start the door it names. */
	void bump(int wall, std::vector<Notice> &notices);

	int getState(uint16 id) const;
	int getPosition(uint16 id) const;

	/** Save or load every mover's position and state; loading puts the parts back. */
	void sync(Common::Serializer &s, Walker &player, std::vector<int> &moved);

private:
	struct Part {
		bool wall;
		int index;
		int mode;
		// The original geometry.
		int32 x1, y1, x2, y2;
		int32 top, bottom, vStart, vLength;
		int32 height;
	};
	struct Leader {
		Part part;
		std::vector<Part> followers;
	};
	struct Mover {
		uint16 id = 0;
		std::vector<Leader> leaders;
		uint16 rate = 0x4000;
		uint16 wait = 0x100;
		int position = 0;
		int state = kIdle, lastState = kIdle;
		uint32 counter = 0;
		uint32 flags = 0;
	};
	enum {
		kFlagAutoClose = 1,
		kFlagLocked = 2,
		kFlagReverses = 4,
		kFlagOpenedBack = 8
	};

	Mover *find(uint16 id);
	const Mover *find(uint16 id) const;
	Part makePart(bool wall, int index, int mode) const;
	/**
	 * Move every part to position; with check, false (and nothing moved)
	 * if a part would run into the player, who rides a moving floor.
	 */
	bool apply(Mover &mover, int position, Walker &player, std::vector<int> &moved, bool check);
	/** Announce a state change, as 0x4B7980 does. */
	void changed(Mover &mover, std::vector<Notice> &notices);

	LevelFile &_level;
	std::vector<Mover> _movers;
	std::map<uint16, int> _byId;
};

} // End of namespace STG

#endif
