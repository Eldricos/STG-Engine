/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "sdl/sdl-events.h"
#include "sdl/sdl-graphics.h"

#include "common/config-manager.h"

bool SdlEventSource::pollEvent(Common::Event &event) {
	SDL_Event sdlEvent;
	while (SDL_PollEvent(&sdlEvent)) {
		event = Common::Event();
		if (translate(sdlEvent, event))
			return true;
	}
	return false;
}

bool SdlEventSource::translate(SDL_Event &sdlEvent, Common::Event &event) {
	_graphics->convertEventCoordinates(sdlEvent);

	switch (sdlEvent.type) {
	case SDL_EVENT_QUIT:
	case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
		event.type = Common::EVENT_QUIT;
		return true;

	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_KEY_UP: {
		const SDL_KeyboardEvent &key = sdlEvent.key;
		if (sdlEvent.type == SDL_EVENT_KEY_DOWN && handleHotkey(key))
			return false;
		event.type = sdlEvent.type == SDL_EVENT_KEY_DOWN ? Common::EVENT_KEYDOWN : Common::EVENT_KEYUP;
		event.kbdRepeat = key.repeat;
		event.kbd.keycode = mapKey(key.key);
		event.kbd.flags = mapModifiers(key.mod);
		event.kbd.ascii = mapAscii(key);
		return event.kbd.keycode != Common::KEYCODE_INVALID || event.kbd.ascii != 0;
	}

	case SDL_EVENT_MOUSE_MOTION:
		event.type = Common::EVENT_MOUSEMOVE;
		event.mouse = Common::Point((int16)sdlEvent.motion.x, (int16)sdlEvent.motion.y);
		_graphics->setMousePos(event.mouse.x, event.mouse.y);
		return true;

	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP: {
		bool down = sdlEvent.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
		event.mouse = Common::Point((int16)sdlEvent.button.x, (int16)sdlEvent.button.y);
		_graphics->setMousePos(event.mouse.x, event.mouse.y);
		switch (sdlEvent.button.button) {
		case SDL_BUTTON_LEFT:
			event.type = down ? Common::EVENT_LBUTTONDOWN : Common::EVENT_LBUTTONUP;
			return true;
		case SDL_BUTTON_RIGHT:
			event.type = down ? Common::EVENT_RBUTTONDOWN : Common::EVENT_RBUTTONUP;
			return true;
		case SDL_BUTTON_MIDDLE:
			event.type = down ? Common::EVENT_MBUTTONDOWN : Common::EVENT_MBUTTONUP;
			return true;
		default:
			return false;
		}
	}

	case SDL_EVENT_MOUSE_WHEEL: {
		float y = sdlEvent.wheel.y;
		if (sdlEvent.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
			y = -y;
		if (y == 0)
			return false;
		event.type = y > 0 ? Common::EVENT_WHEELUP : Common::EVENT_WHEELDOWN;
		event.mouse = Common::Point((int16)sdlEvent.wheel.mouse_x, (int16)sdlEvent.wheel.mouse_y);
		return true;
	}

	case SDL_EVENT_WINDOW_FOCUS_GAINED:
		event.type = Common::EVENT_FOCUS_GAINED;
		return true;
	case SDL_EVENT_WINDOW_FOCUS_LOST:
		event.type = Common::EVENT_FOCUS_LOST;
		return true;

	case SDL_EVENT_WINDOW_RESIZED:
	case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
	case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
		event.type = Common::EVENT_SCREEN_CHANGED;
		return true;

	default:
		return false;
	}
}

bool SdlEventSource::handleHotkey(const SDL_KeyboardEvent &key) {
	if (key.repeat)
		return false;

	// Alt+Enter: toggle fullscreen, as in most Windows games.
	if ((key.mod & SDL_KMOD_ALT) && (key.key == SDLK_RETURN || key.key == SDLK_KP_ENTER)) {
		_graphics->setFullscreen(!_graphics->isFullscreen());
		ConfMan.setBool("fullscreen", _graphics->isFullscreen());
		return true;
	}

	// Ctrl+Alt+F: toggle bilinear filtering.
	if ((key.mod & SDL_KMOD_ALT) && (key.mod & SDL_KMOD_CTRL) && key.key == SDLK_F) {
		_graphics->setFiltering(!_graphics->isFiltering());
		ConfMan.setBool("filtering", _graphics->isFiltering());
		return true;
	}

	return false;
}

Common::KeyCode SdlEventSource::mapKey(SDL_Keycode key) {
	// SDL keycodes for printable keys are their (lower-case) characters,
	// which is also how Common::KeyCode is laid out.
	if ((key >= 32 && key < 127) || key == SDLK_BACKSPACE || key == SDLK_TAB || key == SDLK_RETURN ||
	    key == SDLK_ESCAPE || key == SDLK_DELETE)
		return (Common::KeyCode)key;

	switch (key) {
	case SDLK_KP_0: return Common::KEYCODE_KP0;
	case SDLK_KP_1: return Common::KEYCODE_KP1;
	case SDLK_KP_2: return Common::KEYCODE_KP2;
	case SDLK_KP_3: return Common::KEYCODE_KP3;
	case SDLK_KP_4: return Common::KEYCODE_KP4;
	case SDLK_KP_5: return Common::KEYCODE_KP5;
	case SDLK_KP_6: return Common::KEYCODE_KP6;
	case SDLK_KP_7: return Common::KEYCODE_KP7;
	case SDLK_KP_8: return Common::KEYCODE_KP8;
	case SDLK_KP_9: return Common::KEYCODE_KP9;
	case SDLK_KP_PERIOD: return Common::KEYCODE_KP_PERIOD;
	case SDLK_KP_DIVIDE: return Common::KEYCODE_KP_DIVIDE;
	case SDLK_KP_MULTIPLY: return Common::KEYCODE_KP_MULTIPLY;
	case SDLK_KP_MINUS: return Common::KEYCODE_KP_MINUS;
	case SDLK_KP_PLUS: return Common::KEYCODE_KP_PLUS;
	case SDLK_KP_ENTER: return Common::KEYCODE_KP_ENTER;
	case SDLK_UP: return Common::KEYCODE_UP;
	case SDLK_DOWN: return Common::KEYCODE_DOWN;
	case SDLK_LEFT: return Common::KEYCODE_LEFT;
	case SDLK_RIGHT: return Common::KEYCODE_RIGHT;
	case SDLK_INSERT: return Common::KEYCODE_INSERT;
	case SDLK_HOME: return Common::KEYCODE_HOME;
	case SDLK_END: return Common::KEYCODE_END;
	case SDLK_PAGEUP: return Common::KEYCODE_PAGEUP;
	case SDLK_PAGEDOWN: return Common::KEYCODE_PAGEDOWN;
	case SDLK_F1: return Common::KEYCODE_F1;
	case SDLK_F2: return Common::KEYCODE_F2;
	case SDLK_F3: return Common::KEYCODE_F3;
	case SDLK_F4: return Common::KEYCODE_F4;
	case SDLK_F5: return Common::KEYCODE_F5;
	case SDLK_F6: return Common::KEYCODE_F6;
	case SDLK_F7: return Common::KEYCODE_F7;
	case SDLK_F8: return Common::KEYCODE_F8;
	case SDLK_F9: return Common::KEYCODE_F9;
	case SDLK_F10: return Common::KEYCODE_F10;
	case SDLK_F11: return Common::KEYCODE_F11;
	case SDLK_F12: return Common::KEYCODE_F12;
	case SDLK_NUMLOCKCLEAR: return Common::KEYCODE_NUMLOCK;
	case SDLK_CAPSLOCK: return Common::KEYCODE_CAPSLOCK;
	case SDLK_SCROLLLOCK: return Common::KEYCODE_SCROLLOCK;
	case SDLK_LSHIFT: return Common::KEYCODE_LSHIFT;
	case SDLK_RSHIFT: return Common::KEYCODE_RSHIFT;
	case SDLK_LCTRL: return Common::KEYCODE_LCTRL;
	case SDLK_RCTRL: return Common::KEYCODE_RCTRL;
	case SDLK_LALT: return Common::KEYCODE_LALT;
	case SDLK_RALT: return Common::KEYCODE_RALT;
	case SDLK_PRINTSCREEN: return Common::KEYCODE_PRINT;
	case SDLK_PAUSE: return Common::KEYCODE_PAUSE;
	default: return Common::KEYCODE_INVALID;
	}
}

byte SdlEventSource::mapModifiers(SDL_Keymod mod) {
	byte flags = 0;
	if (mod & SDL_KMOD_CTRL)
		flags |= Common::KBD_CTRL;
	if (mod & SDL_KMOD_ALT)
		flags |= Common::KBD_ALT;
	if (mod & SDL_KMOD_SHIFT)
		flags |= Common::KBD_SHIFT;
	if (mod & SDL_KMOD_GUI)
		flags |= Common::KBD_META;
	if (mod & SDL_KMOD_NUM)
		flags |= Common::KBD_NUM;
	if (mod & SDL_KMOD_CAPS)
		flags |= Common::KBD_CAPS;
	return flags;
}

uint16 SdlEventSource::mapAscii(const SDL_KeyboardEvent &key) {
	switch (key.key) {
	case SDLK_BACKSPACE:
		return 8;
	case SDLK_TAB:
		return 9;
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		return 13;
	case SDLK_ESCAPE:
		return 27;
	default:
		break;
	}

	// Ask SDL for the key as modified by Shift/Caps in the current layout.
	SDL_Keycode shifted = SDL_GetKeyFromScancode(key.scancode, key.mod, true);
	if (shifted >= 32 && shifted < 127)
		return (uint16)shifted;
	return 0;
}
