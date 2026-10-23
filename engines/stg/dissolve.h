/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_DISSOLVE_H
#define STG_DISSOLVE_H

#include "common/graphics/surface.h"
#include "common/random.h"

namespace STG {

/**
 * The mission transitions' blends (the display's slots 0x64, 0x68 and 0x6C),
 * on 8-bit surfaces of the same size. Amounts are in 1024ths: how much of
 * each row is changed.
 */
namespace Dissolve {

/**
 * 0x40BC05: speckles of a colour over the surface. Style 1 sets single
 * pixels at random, style 2 short streaks (a sixteenth of the width) on
 * random rows. Past half, the colour fills the surface and the picture
 * shows through instead.
 */
void speckle(Graphics::Surface &surface, byte colour, int amount, int style, Common::RandomSource &random);

/**
 * 0x40C852: from one picture to another. Up to half, the first with that
 * share of each row's pixels taken from the second at random; past it, the
 * second with the rest taken from the first. The surface may be the first
 * picture, not the second.
 */
void crossfade(Graphics::Surface &surface, const Graphics::Surface &from, const Graphics::Surface &to, int amount,
               Common::RandomSource &random);

} // End of namespace Dissolve

} // End of namespace STG

#endif
