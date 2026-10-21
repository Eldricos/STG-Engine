/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "common/graphics/pixelformat.h"

namespace Graphics {

Common::String PixelFormat::toString() const {
	if (isCLUT8())
		return "CLUT8";
	return Common::format("RGBA%d%d%d%d@%d", 8 - rLoss, 8 - gLoss, 8 - bLoss, 8 - aLoss, bytesPerPixel * 8);
}

} // End of namespace Graphics
