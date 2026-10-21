/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "common/events.h"
#include "common/system.h"

namespace Common {

bool EventManager::pollEvent(Event &event) {
	if (!_queue.empty()) {
		event = _queue.front();
		_queue.pop_front();
	} else if (!_system->pollEvent(event)) {
		return false;
	}

	switch (event.type) {
	case EVENT_KEYDOWN:
	case EVENT_KEYUP:
		_modifierState = event.kbd.flags;
		break;
	case EVENT_MOUSEMOVE:
	case EVENT_WHEELUP:
	case EVENT_WHEELDOWN:
		_mousePos = event.mouse;
		break;
	case EVENT_LBUTTONDOWN:
		_mousePos = event.mouse;
		_buttonState |= 1;
		break;
	case EVENT_LBUTTONUP:
		_mousePos = event.mouse;
		_buttonState &= ~1;
		break;
	case EVENT_RBUTTONDOWN:
		_mousePos = event.mouse;
		_buttonState |= 2;
		break;
	case EVENT_RBUTTONUP:
		_mousePos = event.mouse;
		_buttonState &= ~2;
		break;
	case EVENT_MBUTTONDOWN:
		_mousePos = event.mouse;
		_buttonState |= 4;
		break;
	case EVENT_MBUTTONUP:
		_mousePos = event.mouse;
		_buttonState &= ~4;
		break;
	case EVENT_QUIT:
		_shouldQuit = true;
		break;
	case EVENT_RETURN_TO_LAUNCHER:
		_shouldReturnToLauncher = true;
		break;
	default:
		break;
	}
	return true;
}

void EventManager::pushEvent(const Event &event) {
	_queue.push_back(event);
}

} // End of namespace Common
