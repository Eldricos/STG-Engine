/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_EVENTS_H
#define COMMON_EVENTS_H

#include <deque>

#include "common/keyboard.h"
#include "common/rect.h"

class OSystem;

namespace Common {

enum EventType {
	EVENT_INVALID = 0,
	EVENT_KEYDOWN,
	EVENT_KEYUP,
	EVENT_MOUSEMOVE,
	EVENT_LBUTTONDOWN,
	EVENT_LBUTTONUP,
	EVENT_RBUTTONDOWN,
	EVENT_RBUTTONUP,
	EVENT_MBUTTONDOWN,
	EVENT_MBUTTONUP,
	EVENT_WHEELUP,
	EVENT_WHEELDOWN,
	/** The user asked to close the window. */
	EVENT_QUIT,
	/** The engine should stop and return to the launcher. */
	EVENT_RETURN_TO_LAUNCHER,
	EVENT_FOCUS_GAINED,
	EVENT_FOCUS_LOST,
	/** The window was resized or toggled fullscreen; nothing to redraw in game coordinates. */
	EVENT_SCREEN_CHANGED
};

struct Event {
	EventType type = EVENT_INVALID;
	/** True for auto-repeated key down events. */
	bool kbdRepeat = false;
	KeyState kbd;
	/** Mouse position in game screen coordinates. */
	Point mouse;
};

/**
 * Collects events from the backend and keeps track of input state.
 * Engines poll this; they never see the host's own event types.
 */
class EventManager {
public:
	explicit EventManager(OSystem *system) : _system(system) {}

	/** Fetch the next event; returns false when none is pending. */
	bool pollEvent(Event &event);

	/** Push an event to be returned by the next pollEvent call. */
	void pushEvent(const Event &event);

	Point getMousePos() const { return _mousePos; }
	/** Bit 0: left, bit 1: right, bit 2: middle. */
	int getButtonState() const { return _buttonState; }
	int getModifierState() const { return _modifierState; }

	bool shouldQuit() const { return _shouldQuit; }
	bool shouldReturnToLauncher() const { return _shouldReturnToLauncher; }
	void resetQuit() { _shouldQuit = false; }
	void resetReturnToLauncher() { _shouldReturnToLauncher = false; }

private:
	OSystem *_system;
	std::deque<Event> _queue;
	Point _mousePos;
	int _buttonState = 0;
	int _modifierState = 0;
	bool _shouldQuit = false;
	bool _shouldReturnToLauncher = false;
};

} // End of namespace Common

#endif
