/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SDL_SDL_EVENTS_H
#define SDL_SDL_EVENTS_H

#include <SDL3/SDL.h>

#include "common/events.h"

class SdlGraphicsManager;

/** Translates SDL events into engine-facing Common::Event values. */
class SdlEventSource {
public:
	explicit SdlEventSource(SdlGraphicsManager *graphics) : _graphics(graphics) {}

	/** Returns false when no more events are pending. */
	bool pollEvent(Common::Event &event);

private:
	/** Returns true if the SDL event produced a Common event. */
	bool translate(SDL_Event &sdlEvent, Common::Event &event);
	/** Backend hotkeys such as Alt+Enter; returns true if consumed. */
	bool handleHotkey(const SDL_KeyboardEvent &key);

	static Common::KeyCode mapKey(SDL_Keycode key);
	static byte mapModifiers(SDL_Keymod mod);
	static uint16 mapAscii(const SDL_KeyboardEvent &key);

	SdlGraphicsManager *_graphics;
};

#endif
