/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef ENGINES_LAUNCHER_H
#define ENGINES_LAUNCHER_H

class OSystem;

/**
 * The platform independent main function, equivalent to ScummVM's
 * scummvm_main(). The backend's real main() constructs its OSystem and
 * hands control over to this.
 */
int stgMain(OSystem &system, int argc, char *argv[]);

#endif
