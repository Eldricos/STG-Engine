/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <SDL3/SDL.h>
// Provides WinMain and friends where the platform needs them.
#include <SDL3/SDL_main.h>

#include "engines/launcher.h"
#include "sdl/sdl-system.h"

int main(int argc, char *argv[]) {
	OSystem_SDL system;
	return stgMain(system, argc, argv);
}
