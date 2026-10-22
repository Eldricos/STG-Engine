/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/hudscreen.h"

#include <algorithm>
#include <cmath>

#include "common/image/bmp.h"
#include "common/str.h"
#include "engines/stg/level.h"
#include "engines/stg/mission.h"
#include "engines/stg/script.h"

namespace STG {

namespace {

const double kPi = 3.14159265358979323846;

/** Inclusive rectangles, as the original's tables give them. */
Common::Rect inclusive(int x1, int y1, int x2, int y2) {
	return Common::Rect(x1, y1, x2 + 1, y2 + 1);
}

// The console's parts, as the original lays them out (see doc/formats.md).
const Common::Rect kLeftPanel = inclusive(0, 253, 205, 458);
const Common::Rect kTabs[3] = { inclusive(6, 261, 28, 312), inclusive(6, 331, 28, 382), inclusive(6, 401, 28, 452) };
const Common::Rect kMap = inclusive(39, 267, 192, 414);
const Common::Rect kViewer = inclusive(223, 311, 433, 458);
const Common::Rect kAuxCycle = inclusive(207, 425, 219, 459);
const Common::Rect kWeaponCycle = inclusive(207, 253, 219, 268);
const Common::Rect kLevelsButton = Common::Rect(43, 415, 43 + 29, 415 + 27);
const Common::Rect kZoomButton = Common::Rect(74, 415, 74 + 29, 415 + 27);
const int kSlotX[3] = { 436, 505, 574 };
const int kSlotY[4] = { 257, 308, 359, 410 };
const int kIconWidth = 64, kIconHeight = 46;
/** The rectangle round the item in use: palette index 0x22. */
const byte kActiveColour = 0x22;
const char *const kMarkerStyles[] = { "romu", "sorn", "injr", "klng", "chod", "fish", "fuzy", "pred" };
const double kMapScales[] = { 0.5, 1.0, 2.0 };

Common::String normalise(const Common::String &name) {
	Common::String s = Common::toLower(name);
	std::replace(s.begin(), s.end(), '\\', '/');
	return s;
}

} // End of anonymous namespace

const Common::Rect HudScreen::kView = Common::Rect(0, 0, 640, 252);
const Common::Rect HudScreen::kTextLine = inclusive(10, 461, 639, 477);

int HudScreen::Image::getFrameCount() const {
	return frameHeight > 0 ? std::max(1, (int)surface.h / frameHeight) : 1;
}

HudScreen::HudScreen(const ResourceManager &resources, const ResourceRef &level, const ScriptFile &script)
	: _resources(resources), _level(level), _script(script) {
	_console = load("actionx.bmp", kGlobal);
}

HudScreen::~HudScreen() {}

const HudScreen::Image *HudScreen::load(const Common::String &name, Scope scope, int frameHeight) {
	Common::String key = Common::String(scope == kGlobal ? "g:" : "m:") + normalise(name);
	auto it = _images.find(key);
	if (it != _images.end())
		return it->second.get();

	// Global names are found in COMMON.DAT wherever they sit in it; the
	// mission's beside its GAME.LV2.
	Common::String wanted = normalise(name);
	size_t slash = _level.name.find_last_of("/\\");
	Common::String folder = slash == Common::String::npos ? "" : normalise(_level.name.substr(0, slash + 1));
	const ResourceRef *found = nullptr;
	for (const ResourceRef &ref : _resources.getImages()) {
		Common::String candidate = normalise(ref.name);
		bool match;
		if (scope == kGlobal) {
			match = Common::toUpper(ref.archive) == "COMMON.DAT" &&
			        (candidate == wanted || Common::hasSuffixIgnoreCase(candidate, "/" + wanted));
		} else {
			match = ref.archive == _level.archive && candidate == folder + wanted;
		}
		if (match) {
			found = &ref;
			break;
		}
	}
	std::unique_ptr<Image> image;
	::Image::BitmapDecoder decoder;
	if (found && _resources.loadImage(*found, decoder) && decoder.hasPalette()) {
		image.reset(new Image());
		const Graphics::Surface &s = decoder.getSurface();
		image->surface.create(s.w, s.h, Graphics::PixelFormat::createFormatCLUT8());
		image->surface.copyRectToSurface(s, 0, 0, Common::Rect(s.w, s.h));
		// A negative frame height asks for that many frames.
		image->frameHeight = frameHeight < 0 ? s.h / -frameHeight : frameHeight;
	}
	const Image *result = image.get();
	_images[key] = std::move(image);
	return result;
}

const HudScreen::Image *HudScreen::loadScriptImage(int16 index, int frameHeight) {
	const Common::StringArray &names = _script.getResourceNames();
	if (index < 0 || index >= (int16)names.size())
		return nullptr;
	const Common::String &name = names[index];
	if (!name.empty() && name[0] == '*')
		return load(name.substr(1), kGlobal, frameHeight);
	return load(name, kMission, frameHeight);
}

void HudScreen::blit(Graphics::Surface &dst, const Image &image, int frame, int x, int y, int key,
                     const Common::Rect *clip) {
	int height = image.frameHeight > 0 ? image.frameHeight : image.surface.h;
	frame = std::max(0, std::min(frame, image.getFrameCount() - 1));
	Common::Rect area = clip ? *clip : Common::Rect(dst.w, dst.h);
	for (int row = 0; row < height; row++) {
		int sy = y + row;
		if (sy < area.top || sy >= area.bottom)
			continue;
		const byte *src = (const byte *)image.surface.getBasePtr(0, frame * height + row);
		byte *out = (byte *)dst.getBasePtr(0, sy);
		for (int col = 0; col < image.surface.w; col++) {
			int sx = x + col;
			if (sx < area.left || sx >= area.right || src[col] == key)
				continue;
			out[sx] = src[col];
		}
	}
}

void HudScreen::draw(Graphics::Surface &screen, Mission &mission, const LevelFile &level, double playerX,
                     double playerY, double playerZ, double yaw, uint32 time) {
	if (!_console)
		return;
	switch (mission.takeLeftPanelRequest()) {
	case 2:
		_leftMode = kLeftOff;
		break;
	case 3:
		_leftMode = kLeftLife;
		break;
	default:
		break;
	}

	byte black = level.getPalette().findBestColor(0, 0, 0);
	screen.fillRect(Common::Rect(0, kView.bottom, screen.w, screen.h), black);
	blit(screen, *_console, 0, 206, 252);
	if (const Image *left = load("l-m-m-o.bmp", kGlobal, 206))
		blit(screen, *left, (int)_leftMode, kLeftPanel.left, kLeftPanel.top);

	switch (_leftMode) {
	case kLeftLife:
		drawLifePanel(screen, mission);
		break;
	case kLeftMap:
		drawMap(screen, mission, level, playerX, playerY, playerZ, yaw);
		break;
	case kLeftObject:
		if (const Image *object = load("OBJECT.BMP", kMission))
			blit(screen, *object, 0, 39, 267);
		break;
	default:
		break;
	}

	if (const Image *cycle = load("wepcycle.bmp", kGlobal))
		blit(screen, *cycle, 0, kWeaponCycle.left, kWeaponCycle.top);
	// The weapon in use shows its display (0x439F9E); the phaser's is weapphas.bmp.
	if (mission.getActiveItem() && mission.getItemKind(mission.getActiveItem()) == 2) {
		if (const Image *display = load("weapphas.bmp", kGlobal))
			blit(screen, *display, 0, 222, 252);
	}
	drawPanel(screen, mission, time);
	drawInventory(screen, mission);
}

void HudScreen::drawLifePanel(Graphics::Surface &screen, Mission &mission) {
	const Hud &hud = mission.getHud();
	const Image *body = hud.getBodyImage() < 0 ? load("BODY.BMP", kMission, 177)
	                                           : loadScriptImage(hud.getBodyImage(), 177);
	if (body)
		blit(screen, *body, hud.getBodyFrame(), 40, 269);

	// The health bar (0x445AFE): a background by how low it is, then a
	// stack of units, 3 rows apart with a gap above the first 15.
	int health = mission.getHealth();
	if (const Image *bar = load("3health.bmp", kGlobal, 50))
		blit(screen, *bar, health > 15 ? 2 : health > 5 ? 1 : 0, 118, 393);
	if (const Image *unit = load("unithlt0.bmp", kGlobal)) {
		for (int k = 0; k < health; k++) {
			int y = 442 - unit->surface.h - 1 - 3 * k - (k >= 15 ? 7 : 0);
			if (y < 270)
				break;
			blit(screen, *unit, 0, 119, y);
		}
	}
}

void HudScreen::drawMap(Graphics::Surface &screen, Mission &mission, const LevelFile &level, double playerX,
                        double playerY, double playerZ, double yaw) {
	const Graphics::Palette &palette = level.getPalette();
	byte wallColour = palette.findBestColor(0, 220, 0);
	byte dark = palette.findBestColor(0, 0, 0);
	screen.fillRect(kMap, dark);
	double cx = (kMap.left + kMap.right) / 2.0, cy = (kMap.top + kMap.bottom) / 2.0;
	auto toScreen = [&](double x, double y, int &sx, int &sy) {
		sx = (int)std::lround(cx + (x - playerX) * _mapScale);
		sy = (int)std::lround(cy - (y - playerY) * _mapScale);
	};

	// Walls marked for the map, in the height band shown (relative to the player).
	const double *levels = mission.getHud().getMapLevels();
	double low = playerZ + levels[_mapBand * 2], high = playerZ + levels[_mapBand * 2 + 1];
	const std::vector<LevelFile::Vertex> &vertices = level.getVertices();
	for (const LevelFile::Wall &wall : level.getWalls()) {
		if (!(wall.flags & LevelFile::kWallOnMap))
			continue;
		if (wall.top / 65536.0 < low || wall.bottom / 65536.0 > high)
			continue;
		int x1, y1, x2, y2;
		toScreen(vertices[wall.vertex1].x / 65536.0, vertices[wall.vertex1].y / 65536.0, x1, y1);
		toScreen(vertices[wall.vertex2].x / 65536.0, vertices[wall.vertex2].y / 65536.0, x2, y2);
		// Plot the line, clipped to the map.
		int steps = std::max(std::abs(x2 - x1), std::abs(y2 - y1));
		for (int i = 0; i <= steps; i++) {
			int x = steps ? x1 + (x2 - x1) * i / steps : x1, y = steps ? y1 + (y2 - y1) * i / steps : y1;
			if (kMap.contains(x, y))
				screen.setPixel(x, y, wallColour);
		}
	}

	for (const auto &entry : mission.getHud().getMarkers()) {
		double x, y, z;
		if (!mission.locate(entry.first, x, y, z))
			continue;
		int sx, sy;
		toScreen(x, y, sx, sy);
		const Image *dot = nullptr;
		if (entry.second.kind == Hud::kMarkerLife && entry.second.style >= 0 && entry.second.style < 8)
			dot = load(Common::String("map/dot-") + kMarkerStyles[entry.second.style] + ".bmp", kGlobal);
		if (dot) {
			blit(screen, *dot, 0, sx - dot->surface.w / 2, sy - dot->surface.h / 2, 0, &kMap);
		} else {
			byte colour = palette.findBestColor(255, entry.second.kind == Hud::kMarkerLife ? 0 : 255, 0);
			for (int dy = -1; dy <= 1; dy++) {
				for (int dx = -1; dx <= 1; dx++) {
					if (kMap.contains(sx + dx, sy + dy))
						screen.setPixel(sx + dx, sy + dy, colour);
				}
			}
		}
	}

	// The player, an arrow in eight headings (agpface2.bmp).
	int octant = (int)std::lround(yaw / (kPi / 4)) & 7;
	if (const Image *face = load("agpface2.bmp", kGlobal, -8)) {
		blit(screen, *face, octant, (int)cx - face->surface.w / 2, (int)cy - face->frameHeight / 2, 0, &kMap);
	} else {
		screen.fillRect(Common::Rect((int)cx - 1, (int)cy - 1, (int)cx + 2, (int)cy + 2),
		                palette.findBestColor(255, 255, 255));
	}

	if (const Image *levelsButton = load("levels.bmp", kGlobal))
		blit(screen, *levelsButton, 0, kLevelsButton.left, kLevelsButton.top);
	if (const Image *zoom = load("zoomz.bmp", kGlobal))
		blit(screen, *zoom, 0, kZoomButton.left, kZoomButton.top);
}

void HudScreen::drawPanel(Graphics::Surface &screen, Mission &mission, uint32 time) {
	const Hud &hud = mission.getHud();
	const Hud::Panel *panel = hud.getVisiblePanel();
	if (!panel) {
		if (const Image *background = load("AUXDEF.BMP", kMission))
			blit(screen, *background, 0, kViewer.left, kViewer.top);
	} else {
		if (const Image *image = loadScriptImage(panel->image, Hud::kPanelHeight))
			blit(screen, *image, panel->frame, kViewer.left, kViewer.top, -1, &kViewer);
		for (const Hud::Button &button : panel->buttons) {
			if (button.hotspot)
				continue;
			if (const Image *image = loadScriptImage(button.image, button.rect.height()))
				blit(screen, *image, button.frame, kViewer.left + button.rect.left, kViewer.top + button.rect.top,
				     button.keyed ? 0xFE : -1, &kViewer);
		}
	}
	// The cycle button blinks while there is more than one panel.
	if (const Image *cycle = load("auxcycle.bmp", kGlobal, kAuxCycle.height())) {
		int frame = hud.getPanelCount() > 1 && (time / 500) % 2 ? 1 : 0;
		blit(screen, *cycle, frame, kAuxCycle.left, kAuxCycle.top);
	}
}

void HudScreen::drawInventory(Graphics::Surface &screen, Mission &mission) {
	const Image *icons = load("INVENT.BMP", kMission, kIconHeight);
	const Hud &hud = mission.getHud();
	for (int slot = 0; slot < Hud::kSlots; slot++) {
		uint16 id = hud.getSlot(slot);
		if (!id)
			continue;
		int x = kSlotX[slot % 3], y = kSlotY[slot / 3];
		int icon = mission.getItemIcon(id);
		if (icons && icon >= 0)
			blit(screen, *icons, icon, x, y);
		if (id == mission.getActiveItem())
			screen.frameRect(Common::Rect(x + 1, y + 1, x + 63, y + 45), kActiveColour);
	}
}

bool HudScreen::click(Mission &mission, int x, int y, bool right) {
	if (!_console || y < kView.bottom)
		return false;
	for (int i = 0; i < 3; i++) {
		if (kTabs[i].contains(x, y)) {
			_leftMode = (LeftMode)(kLeftLife + i);
			return true;
		}
	}
	if (_leftMode == kLeftMap) {
		if (kLevelsButton.contains(x, y)) {
			_mapBand = (_mapBand + 1) % 3;
			return true;
		}
		if (kZoomButton.contains(x, y)) {
			int i = 0;
			while (i < 2 && kMapScales[i] != _mapScale)
				i++;
			_mapScale = kMapScales[(i + 1) % 3];
			return true;
		}
	}
	Hud &hud = mission.getHud();
	for (int slot = 0; slot < Hud::kSlots; slot++) {
		int sx = kSlotX[slot % 3], sy = kSlotY[slot / 3];
		if (Common::Rect(sx - 2, sy - 2, sx + kIconWidth + 4, sy + kIconHeight + 4).contains(x, y)) {
			if (right)
				mission.describeItem(slot);
			else
				mission.useItem(slot);
			return true;
		}
	}
	if (kViewer.contains(x, y)) {
		if (right)
			hud.describePanel();
		else
			hud.clickPanel(x - kViewer.left, y - kViewer.top);
		return true;
	}
	if (kAuxCycle.contains(x, y)) {
		hud.nextPanel();
		return true;
	}
	if (kWeaponCycle.contains(x, y)) {
		// The next item the player can hold, after the one in use.
		int current = hud.findItem(mission.getActiveItem());
		for (int i = 1; i <= Hud::kSlots; i++) {
			int slot = (current + i + Hud::kSlots) % Hud::kSlots;
			if (hud.getSlot(slot) && mission.isWeapon(hud.getSlot(slot))) {
				mission.useItem(slot);
				break;
			}
		}
		return true;
	}
	return kLeftPanel.contains(x, y);
}

} // End of namespace STG
