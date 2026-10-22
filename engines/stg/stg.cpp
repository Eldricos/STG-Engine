/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/stg.h"

#include <cctype>
#include <cmath>

#include "common/audio/raw.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/savefile.h"
#include "common/serializer.h"
#include "common/system.h"
#include "engines/stg/locations.h"
#include "engines/stg/movie.h"

namespace STG {

namespace {

const uint32 kFrameMillis = 1000 / 60;

// Palette layout used by the viewer when no game image is shown.
const byte kColorBlack = 0;
const byte kColorStarFirst = 1; // 1..32: grey ramp for stars
const byte kColorStarLast = 32;
const byte kColorCubeFirst = 33; // 33..248: 6x6x6 colour cube for truecolour images
const byte kColorWhite = 255;
const byte kColorFrame = 254;

const int kNumStars = 400;
const int kTextPanelHeight = 60;

// Pages of a level in the viewer.
const int kLevelPageMap = 0;
const int kLevelPage3D = 1;
const int kLevelPageTextures = 2;
const double kPi = 3.14159265358979323846;

} // End of anonymous namespace

STGEngine::STGEngine(OSystem *syst, const DetectedGame &game)
	: Engine(syst, game.path), _game(game), _random(0) {
}

STGEngine::~STGEngine() {
}

Common::Error STGEngine::run() {
	_screen.reset(new Screen(_system));
	_screen->init();
	initPalette();
	initCursor();

	_resources.addSearchPaths(_gameDataDir, ConfMan.get("cd1path"), ConfMan.get("cd2path"));
	_resources.checkDataFiles();
	_resources.openArchives();
	_resources.scan();
	_resources.logInventory();
	loadFont();
	for (const Location &location : getLocations()) {
		ResourceRef ref;
		if (findLocationLevel(_resources, location, ref))
			_missions.push_back({ location.folder, ref });
	}

	if (_game.unknownVariant)
		warning("This version of %s is not known; only the resource viewer is available", _game.description.c_str());

	info("%d images and %d sounds found", (int)_resources.getImages().size(), (int)_resources.getSounds().size());
	info("Controls: Left/Right = image, PgUp/PgDn = archive, Up/Down = sound, Space = replay sound,");
	info("          Home = starfield, click = test tone, F5/F7 = save/load, Alt+Enter = fullscreen, Esc = quit");
	info("          On a level: [ ] = floor plan, 3D view, textures; in 3D, arrows/WASD move, PgUp/PgDn look, L lighting, C collision");
	_status = "Up/Down, Enter: missions   Left/Right: images   PgUp/PgDn: archives   Tab: levels   Esc: quit";

	for (int i = 0; i < kNumStars; i++)
		_stars.push_back({ (int32)_random.getRandomNumberRng(0, 2000) - 1000,
		                   (int32)_random.getRandomNumberRng(0, 2000) - 1000,
		                   (int32)_random.getRandomNumberRng(1, 1000) });

	startFrontEnd();

	uint32 lastFrame = _system->getMillis();
	while (!shouldQuit()) {
		Common::Event event;
		while (_eventMan->pollEvent(event))
			handleEvent(event);

		uint32 now = _system->getMillis();
		uint32 delta = now - lastFrame;
		lastFrame = now;

		if (!isPaused())
			drawFrame(delta);

		uint32 elapsed = _system->getMillis() - now;
		if (elapsed < kFrameMillis)
			_system->delayMillis(kFrameMillis - elapsed);
	}

	_mixer->stopAll();
	return Common::kNoError;
}

void STGEngine::initPalette() {
	Graphics::Palette &pal = _screen->getPalette();
	pal.set(kColorBlack, 0, 0, 0);
	for (int i = kColorStarFirst; i <= kColorStarLast; i++) {
		byte v = (byte)(i * 255 / kColorStarLast);
		pal.set(i, v, v, (byte)std::min(255, v + 24));
	}
	for (int r = 0; r < 6; r++) {
		for (int g = 0; g < 6; g++) {
			for (int b = 0; b < 6; b++)
				pal.set(kColorCubeFirst + r * 36 + g * 6 + b, (byte)(r * 51), (byte)(g * 51), (byte)(b * 51));
		}
	}
	// LCARS-ish orange for the frame.
	pal.set(kColorFrame, 255, 153, 0);
	pal.set(kColorWhite, 255, 255, 255);
	_screen->applyPalette();
}

void STGEngine::initCursor() {
	_targetCursor = true;
	setCursor(false);
	_system->showMouse(true);
}

void STGEngine::setCursor(bool target) {
	// A crosshair to aim with (the original's target.bmp) or an arrow
	// (select.bmp), drawn in the palette's white on its black. Index 0 is
	// transparent.
	if (target == _targetCursor)
		return;
	_targetCursor = target;
	const Graphics::Palette &palette = _screen->getPalette();
	byte white = palette.findBestColor(255, 255, 255), black = palette.findBestColor(0, 0, 0);
	if (!white)
		white = 1;
	if (!black)
		black = white;
	const int size = 15;
	byte cursor[size * size] = {};
	if (target) {
		for (int i = 0; i < size; i++) {
			if (i == size / 2)
				continue;
			cursor[(size / 2) * size + i] = white;
			cursor[i * size + size / 2] = white;
		}
		_system->setMouseCursor(cursor, size, size, size / 2, size / 2, 0);
		return;
	}
	// An arrow 11 rows tall, outlined.
	for (int y = 0; y < 11; y++) {
		int width = y < 8 ? y + 1 : 11 - y;
		for (int x = 0; x < width; x++)
			cursor[y * size + x] = x == 0 || x == width - 1 || y == 10 ? black : white;
	}
	_system->setMouseCursor(cursor, size, size, 0, 0, 0);
}

void STGEngine::loadFont() {
	ResourceRef ref = { "COMMON.DAT", "FONTS/ACTION.FNT" };
	std::unique_ptr<Common::SeekableReadStream> stream = _resources.open(ref);
	if (!stream || !_font.load(*stream))
		warning("Could not load %s; text will not be shown", ref.toString().c_str());

	// The font's colour indices are assumed to refer to the shared palette
	// used by the images in COMMON.DAT.
	_hasFontPalette = _resources.loadArchivePalette("COMMON.DAT", _fontPalette);
	if (!_hasFontPalette)
		warning("No palette found in COMMON.DAT; text colours will be approximate");
}

void STGEngine::handleEvent(const Common::Event &event) {
	if (_menuOpen) {
		handleMenuEvent(event);
		return;
	}
	if (_bridgeMode) {
		switch (event.type) {
		case Common::EVENT_KEYDOWN:
			_bridge->keyDown(event.kbd);
			break;
		case Common::EVENT_MOUSEMOVE:
			_bridge->mouseMove(event.mouse.x, event.mouse.y);
			break;
		case Common::EVENT_LBUTTONDOWN:
			_bridge->mouseDown(event.mouse.x, event.mouse.y);
			break;
		case Common::EVENT_LBUTTONUP:
			_bridge->mouseUp(event.mouse.x, event.mouse.y);
			break;
		default:
			break;
		}
		return;
	}
	bool walking = _level && _levelPage == kLevelPage3D;
	bool bracket = event.kbd.keycode == Common::KEYCODE_LEFTBRACKET || event.kbd.keycode == Common::KEYCODE_RIGHTBRACKET;
	if (walking && bracket && !(event.kbd.flags & Common::KBD_SHIFT) && _mission &&
	    (event.type == Common::EVENT_KEYDOWN || event.type == Common::EVENT_KEYUP)) {
		// The weapon's power level (0x44A1B7 and 0x44A159), if it has one;
		// with Shift, or without, they change pages.
		const Mission::Item *item = _mission->getItem(_mission->getActiveItem());
		if (item && item->maxLevel > 0) {
			if (event.type == Common::EVENT_KEYDOWN) {
				_mission->setItemLevel(item->level + (event.kbd.keycode == Common::KEYCODE_RIGHTBRACKET ? 1 : -1));
				updateWalkStatus();
			}
			return;
		}
	}
	if (walking && (event.type == Common::EVENT_KEYDOWN || event.type == Common::EVENT_KEYUP) &&
	    handleWalkKey(event.kbd.keycode, event.type == Common::EVENT_KEYDOWN))
		return;

	switch (event.type) {
	case Common::EVENT_KEYDOWN:
		switch (event.kbd.keycode) {
		case Common::KEYCODE_ESCAPE:
			// In the game, the menu; in the viewer, out of a mission back
			// to the list, and from anywhere else, quit.
			if (_frontEnd) {
				openMenu(_playingMission);
				break;
			}
			if (_playingMission) {
				leaveMission(Common::format("Left %s", _missions[_missionChoice].name.c_str()));
				break;
			}
			quitGame();
			break;
		case Common::KEYCODE_RETURN:
			if (isOnStarfield() && !_missions.empty())
				playMission(_missionChoice);
			break;
		case Common::KEYCODE_RIGHT:
			showImage(_imageIndex + 1);
			break;
		case Common::KEYCODE_LEFT:
			showImage(_imageIndex - 1);
			break;
		case Common::KEYCODE_PAGEDOWN:
			jumpArchive(1);
			break;
		case Common::KEYCODE_PAGEUP:
			jumpArchive(-1);
			break;
		case Common::KEYCODE_TAB:
			jumpLevel((event.kbd.flags & Common::KBD_SHIFT) ? -1 : 1);
			break;
		case Common::KEYCODE_DOWN:
		case Common::KEYCODE_UP: {
			int step = event.kbd.keycode == Common::KEYCODE_DOWN ? 1 : -1;
			// On the starfield they pick a mission; elsewhere, a sound.
			if (isOnStarfield() && !_missions.empty()) {
				int count = (int)_missions.size();
				_missionChoice = ((_missionChoice + step) % count + count) % count;
				_dirty = true;
			} else {
				playSound(_soundIndex + step);
			}
			break;
		}
		case Common::KEYCODE_SPACE:
			playSound(std::max(_soundIndex, 0));
			break;
		case Common::KEYCODE_HOME:
			showStarfield();
			break;
		case Common::KEYCODE_F8: {
			// Play the movies in turn.
			static const char *const kMovies[] = { "MOVIE/MOV_01.AVI", "MOVIE/MOV_02.AVI" };
			playMovie(kMovies[_movieChoice++ % 2]);
			_dirty = true;
			break;
		}
		case Common::KEYCODE_RIGHTBRACKET:
			showLevelPage(_levelPage + 1);
			break;
		case Common::KEYCODE_LEFTBRACKET:
			showLevelPage(_levelPage - 1);
			break;
		case Common::KEYCODE_F5: {
			Common::Error err = saveGameState(0, "Quick save");
			if (err.failed())
				warning("Save failed: %s", err.getDesc().c_str());
			break;
		}
		case Common::KEYCODE_F7: {
			Common::Error err = loadGameState(0);
			if (err.failed())
				warning("Load failed: %s", err.getDesc().c_str());
			break;
		}
		default:
			break;
		}
		break;
	case Common::EVENT_MOUSEMOVE:
		_mouseX = event.mouse.x;
		_mouseY = event.mouse.y;
		if (_hudScreen)
			_hudScreen->mouseMove(event.mouse.x, event.mouse.y);
		break;
	case Common::EVENT_LBUTTONUP:
		if (_hudScreen && _mission)
			_hudScreen->mouseUp(*_mission, event.mouse.x, event.mouse.y);
		break;
	case Common::EVENT_LBUTTONDOWN:
	case Common::EVENT_RBUTTONDOWN:
		_mouseX = event.mouse.x;
		_mouseY = event.mouse.y;
		if (_level && _levelPage == kLevelPage3D) {
			bool right = event.type == Common::EVENT_RBUTTONDOWN;
			if (_hudScreen && _mission && event.mouse.y >= HudScreen::kView.bottom)
				_hudScreen->mouseDown(*_mission, event.mouse.x, event.mouse.y, right);
			else if (!right)
				attack();
			break;
		}
		if (event.type == Common::EVENT_RBUTTONDOWN)
			break;
		// Pitch rises from left to right across the screen.
		playBeep(220 + event.mouse.x * 2);
		break;
	case Common::EVENT_FOCUS_LOST:
		pauseEngine(true);
		break;
	case Common::EVENT_FOCUS_GAINED:
		pauseEngine(false);
		_dirty = true;
		break;
	default:
		break;
	}
}

void STGEngine::drawFrame(uint32 delta) {
	// Clear the subtitle once its line has finished playing.
	if (_missionSubtitle) {
		if (_levelTime >= _subtitleUntil) {
			_subtitle.clear();
			_missionSubtitle = false;
		}
	} else if (!_subtitle.empty() && !_mixer->isSoundHandleActive(_soundHandle)) {
		_subtitle.clear();
		_dirty = true;
	}

	if (_menuOpen) {
		if (_dirty) {
			_menu->draw(_screen->getSurface());
			_screen->update();
			_dirty = false;
		}
		return;
	}
	if (_bridgeMode) {
		updateBridge(delta);
		return;
	}
	if (_playingMission && _mission && _mission->isFinished() && _frontEnd) {
		campaignMissionEnded();
		return;
	}
	if (_playingMission && _mission && _mission->isFinished()) {
		Common::String message = Common::format("%s %s", _missions[_missionChoice].name.c_str(),
		                                        _mission->isAccomplished() ? "accomplished" : "ended, objectives not met");
		// Codes left for the campaign (SET_MISSION_OUTCOME).
		for (int32 code : _mission->getOutcomes())
			message += Common::format(" [%d]", code);
		leaveMission(message);
		return;
	}
	if (_level && _levelPage == kLevelPage3D && _hudScreen) {
		drawMissionScreen(delta);
		_screen->update();
		_dirty = false;
		return;
	}
	if (_level && _levelPage == kLevelPage3D) {
		_levelTime += delta;
		updateWalk(delta);
		Graphics::Surface view;
		view.create(Screen::kWidth, Screen::kHeight - kTextPanelHeight, Graphics::PixelFormat::createFormatCLUT8());
		_actorSprites.clear();
		if (_actors)
			_actors->getSprites(_camera.x, _camera.y, _walker->getActiveZones(), _actorSprites);
		if (_mission)
			_mission->getProjectiles().getSprites(_actorSprites);
		prepareAim(view.w, view.h);
		_levelRenderer->render(_camera, view, _levelTime);
		drawHands(view, delta);
		if (_mission && _mission->getHealth() != _shownHealth)
			updateWalkStatus();
		_screen->getSurface().copyRectToSurface(view, 0, 0, Common::Rect(view.w, view.h));
		_screen->getSurface().fillRect(Common::Rect(0, view.h, Screen::kWidth, Screen::kHeight),
		                               _screen->getPalette().findBestColor(0, 0, 0));
		applyScreenEffects();
	} else if (_level) {
		if (!_dirty) {
			_system->updateScreen();
			return;
		}
		byte background = _screen->getPalette().findBestColor(0, 0, 0);
		_screen->clear(background);
		if (_levelPage == 0) {
			Graphics::Surface map;
			map.create(Screen::kWidth, Screen::kHeight - kTextPanelHeight, Graphics::PixelFormat::createFormatCLUT8());
			map.fillRect(Common::Rect(map.w, map.h), background);
			drawLevelMap(*_level, map);
			_screen->drawImage(map, Screen::kHeight - kTextPanelHeight);
		} else {
			LevelFile::Texture texture;
			if (_level->loadTexture(_levelTextures[_levelPage - kLevelPageTextures], texture))
				_screen->drawImage(texture.surface, Screen::kHeight - kTextPanelHeight);
		}
	} else if (_spritesLoaded) {
		// Animate at about 8 frames per second, anchored bottom centre.
		_overlayTime += delta;
		if (_overlayTime >= 125) {
			_overlayTime = 0;
			_overlayFrame = (_overlayFrame + 1) % _sprites.getFrameCount();
		}
		const SpriteSet::Frame &f = _sprites.getFrame(_overlayFrame);
		Graphics::Surface frame;
		frame.create(_sprites.getMaxWidth(), _sprites.getMaxHeight(), Graphics::PixelFormat::createFormatCLUT8());
		byte background = _screen->getPalette().findBestColor(0, 0, 0);
		frame.fillRect(Common::Rect(frame.w, frame.h), background);
		frame.transBlitFrom(f.surface, (frame.w - f.surface.w) / 2, frame.h - f.surface.h, SpriteSet::kTransparent);
		_screen->clear(background);
		_screen->drawImage(frame, Screen::kHeight - kTextPanelHeight);
	} else if (_overlayLoaded) {
		// Animate at about 12 frames per second.
		_overlayTime += delta;
		if (_overlayTime >= 80) {
			_overlayTime = 0;
			_overlayFrame = (_overlayFrame + 1) % (_overlay.getRowCount() / _overlayFrameHeight);
		}
		Graphics::Surface frame;
		frame.create(_overlay.getWidth(), _overlayFrameHeight, Graphics::PixelFormat::createFormatCLUT8());
		byte background = _screen->getPalette().findBestColor(0, 0, 0);
		frame.fillRect(Common::Rect(frame.w, frame.h), background);
		_overlay.drawFrame(frame, _overlayFrame, _overlayFrameHeight, 0, 0);
		_screen->clear(background);
		_screen->drawImage(frame, Screen::kHeight - kTextPanelHeight);
	} else if (_imageLoaded) {
		// Static image: only redraw when something changed.
		if (!_dirty) {
			_system->updateScreen();
			return;
		}
		_screen->clear(_screen->getPalette().findBestColor(0, 0, 0));
		_screen->drawImage(_image.getSurface(), Screen::kHeight - kTextPanelHeight);
	} else {
		_screen->clear(kColorBlack);
		drawStarfield(delta);
		Graphics::Surface &s = _screen->getSurface();
		s.frameRect(Common::Rect(8, 8, Screen::kWidth - 8, Screen::kHeight - kTextPanelHeight - 8), kColorFrame);
		drawMissionList();
	}

	drawTextPanel();
	_screen->update();
	_dirty = false;
}

void STGEngine::drawMissionScreen(uint32 delta) {
	_levelTime += delta;
	updateWalk(delta);

	// The view is the top 640 x 252 (the original renders 640 x 320 and
	// shows the middle, the same picture).
	Graphics::Surface view;
	view.create(HudScreen::kView.width(), HudScreen::kView.height(), Graphics::PixelFormat::createFormatCLUT8());
	_actorSprites.clear();
	if (_actors)
		_actors->getSprites(_camera.x, _camera.y, _walker->getActiveZones(), _actorSprites);
	_mission->getProjectiles().getSprites(_actorSprites);
	prepareAim(view.w, view.h);
	_levelRenderer->render(_camera, view, _levelTime);
	drawHands(view, delta);

	// Shaking moves the whole view; the underwater ripple (VIEW_OVERLAY)
	// shifts each row, a wave 168 rows long rolling down a row every 64 ms.
	Hud &hud = _mission->getHud();
	int shakeX, shakeY;
	hud.getShakeOffset(shakeX, shakeY);
	Graphics::Surface &screen = _screen->getSurface();
	byte black = _screen->getPalette().findBestColor(0, 0, 0);
	for (int y = 0; y < view.h; y++) {
		int dx = shakeX;
		if (hud.hasRipple())
			dx += (int)std::lround(5.5 * std::sin(2 * 3.14159265358979 * (y + _levelTime / 64.0) / 168) + 0.5);
		int sy = y - shakeY;
		byte *out = (byte *)screen.getBasePtr(0, y);
		const byte *src = sy >= 0 && sy < view.h ? (const byte *)view.getBasePtr(0, sy) : nullptr;
		for (int x = 0; x < view.w; x++) {
			int sx = x - dx;
			out[x] = src && sx >= 0 && sx < view.w ? src[sx] : black;
		}
	}

	_hudScreen->draw(screen, *_mission, *_level, _walker->getX(), _walker->getY(), _walker->getFeet(), _camera.yaw,
	                 _levelTime);
	_hudScreen->drawDrag(screen, *_mission);
	if (_mission->getHealth() != _shownHealth)
		updateWalkStatus();

	// One line of text: what is said, or else the status.
	if (_font.isLoaded()) {
		const Graphics::Palette &pal = _screen->getPalette();
		byte remap[256];
		for (int i = 0; i < 256; i++) {
			byte r, g, b;
			if (_hasFontPalette)
				_fontPalette.get(i, r, g, b);
			else
				r = g = b = (byte)i;
			remap[i] = pal.findBestColor(r, g, b);
		}
		const Common::String &text = _subtitle.empty() ? _status : _subtitle;
		std::vector<Common::String> lines = _font.wordWrap(text, HudScreen::kTextLine.width());
		if (!lines.empty())
			_font.drawString(screen, lines[0], HudScreen::kTextLine.left, HudScreen::kTextLine.top, remap);
	}
	applyScreenEffects();
}

void STGEngine::applyScreenEffects() {
	if (!_mission || !_level)
		return;
	// 0x43199A: each slot blends every colour towards its own, slot 0 last.
	Hud &hud = _mission->getHud();
	bool any = false;
	Graphics::Palette blended = _level->getPalette();
	for (int slot = Hud::kEffectSlots - 1; slot >= 0; slot--) {
		byte r, g, b;
		int k = hud.getEffectWeight(slot, r, g, b);
		if (k <= 0)
			continue;
		any = true;
		for (int i = 0; i < 256; i++) {
			byte pr, pg, pb;
			blended.get(i, pr, pg, pb);
			blended.set(i, (byte)((pr * (128 - k) + r * k) >> 7), (byte)((pg * (128 - k) + g * k) >> 7),
			            (byte)((pb * (128 - k) + b * k) >> 7));
		}
	}

	// A transition (0x91, 0x92): the briefing picture, or the game, faded.
	Mission::TransitionView transition = _mission->getTransitionView();
	if (transition.active) {
		any = true;
		if (transition.briefing && _briefingLoaded) {
			const Graphics::Surface &picture = _briefing.getSurface();
			Graphics::Surface &screen = _screen->getSurface();
			screen.fillRect(Common::Rect(screen.w, screen.h), 0);
			screen.copyRectToSurface(picture, std::max(0, (screen.w - picture.w) / 2),
			                         std::max(0, (screen.h - picture.h) / 2),
			                         Common::Rect(std::min<int>(picture.w, screen.w), std::min<int>(picture.h, screen.h)));
			blended = Graphics::Palette();
			blended.set(_briefing.getPalette().data(), 0, _briefing.getPaletteColorCount());
		} else if (transition.briefing) {
			transition.brightness = 0;
		}
		double k = transition.brightness;
		for (int i = 0; i < 256; i++) {
			byte r, g, b;
			blended.get(i, r, g, b);
			blended.set(i, (byte)(r * k + transition.r * (1 - k)), (byte)(g * k + transition.g * (1 - k)),
			            (byte)(b * k + transition.b * (1 - k)));
		}
	}
	if (any || _paletteBlended) {
		_screen->getPalette() = blended;
		_screen->applyPalette();
	}
	_paletteBlended = any;
}

void STGEngine::drawTextPanel() {
	if (!_font.isLoaded())
		return;

	// Translate the font's colours into whatever palette is current, so
	// text keeps its look over every image.
	const Graphics::Palette &pal = _screen->getPalette();
	byte remap[256];
	for (int i = 0; i < 256; i++) {
		byte r, g, b;
		if (_hasFontPalette)
			_fontPalette.get(i, r, g, b);
		else
			r = g = b = (byte)i;
		remap[i] = pal.findBestColor(r, g, b);
	}
	byte background = pal.findBestColor(0, 0, 0);
	byte highlight = pal.findBestColor(255, 153, 0);

	Graphics::Surface &s = _screen->getSurface();
	int top = Screen::kHeight - kTextPanelHeight;
	s.fillRect(Common::Rect(0, top, Screen::kWidth, Screen::kHeight), background);
	s.hLine(0, top, Screen::kWidth - 1, highlight);

	const int margin = 8;
	int y = top + 4;
	_font.drawString(s, _status, margin, y, remap);
	y += _font.getHeight();
	for (const Common::String &line : _font.wordWrap(_subtitle, Screen::kWidth - 2 * margin)) {
		if (y + _font.getHeight() > Screen::kHeight)
			break;
		_font.drawString(s, line, margin, y, remap);
		y += _font.getHeight();
	}
}

bool STGEngine::isOnStarfield() const {
	return !_level && !_imageLoaded && !_overlayLoaded && !_spritesLoaded;
}

void STGEngine::drawMissionList() {
	if (!_font.isLoaded() || _missions.empty())
		return;
	const Graphics::Palette &pal = _screen->getPalette();
	byte remap[256];
	for (int i = 0; i < 256; i++) {
		byte r, g, b;
		if (_hasFontPalette)
			_fontPalette.get(i, r, g, b);
		else
			r = g = b = (byte)i;
		remap[i] = pal.findBestColor(r, g, b);
	}
	Graphics::Surface &s = _screen->getSurface();
	int x = 40, y = 32;
	_font.drawString(s, "Away missions (Up/Down, Enter):", x, y, remap);
	y += _font.getHeight() * 3 / 2;
	for (int i = 0; i < (int)_missions.size(); i++) {
		if (i == _missionChoice)
			s.frameRect(Common::Rect(x - 4, y - 1, x + 160, y + _font.getHeight() + 1), kColorFrame);
		_font.drawString(s, _missions[i].name, x, y, remap);
		y += _font.getHeight() + 2;
	}
	if (!_missionMessage.empty())
		_font.drawString(s, _missionMessage, x, y + _font.getHeight(), remap);
}

void STGEngine::playMission(int index) {
	_missionChoice = index;
	_missionMessage.clear();
	_subtitle.clear();
	_missionSubtitle = false;
	showLevel(_missions[index].level);
	if (!_level)
		return;
	_playingMission = true;
	showLevelPage(kLevelPage3D);
}

void STGEngine::leaveMission(const Common::String &message) {
	if (_mission)
		_carriedArray = _mission->getCarriedArray();
	_playingMission = false;
	_subtitle.clear();
	_missionSubtitle = false;
	_missionMessage = message;
	info("%s", message.c_str());
	_mixer->stopAll();
	showStarfield();
}

void STGEngine::drawStarfield(uint32 delta) {
	Graphics::Surface &s = _screen->getSurface();
	const int cx = Screen::kWidth / 2, cy = Screen::kHeight / 2;
	int32 speed = (int32)std::max<uint32>(1, delta / 2);

	for (Star &star : _stars) {
		star.z -= speed;
		if (star.z <= 1) {
			star.x = (int32)_random.getRandomNumberRng(0, 2000) - 1000;
			star.y = (int32)_random.getRandomNumberRng(0, 2000) - 1000;
			star.z = 1000;
		}
		int sx = cx + star.x * 256 / star.z;
		int sy = cy + star.y * 256 / star.z;
		byte color = (byte)(kColorStarFirst + (kColorStarLast - kColorStarFirst) * (1000 - star.z) / 1000);
		s.setPixel(sx, sy, color);
		if (star.z < 300) {
			s.setPixel(sx + 1, sy, color);
			s.setPixel(sx, sy + 1, color);
			s.setPixel(sx + 1, sy + 1, color);
		}
	}
}

void STGEngine::showImage(int index) {
	const std::vector<ResourceRef> &images = _resources.getImages();
	if (images.empty())
		return;

	int count = (int)images.size();
	index = ((index % count) + count) % count;
	_imageIndex = index;
	const ResourceRef &ref = images[index];
	_imageLoaded = _overlayLoaded = _spritesLoaded = false;
	_hudScreen.reset();
	_heldItem.load(_resources, _levelRef, 0);
	_mission.reset();
	_actors.reset();
	_walker.reset();
	_levelRenderer.reset();
	_level.reset();

	if (Common::hasSuffixIgnoreCase(ref.name, ".lv2")) {
		showLevel(ref);
	} else if (Common::hasSuffixIgnoreCase(ref.name, ".ovr")) {
		showOverlay(ref);
	} else if (Common::hasSuffixIgnoreCase(ref.name, ".gs2")) {
		showSprites(ref);
	} else {
		_imageLoaded = _resources.loadImage(ref, _image);
		if (_imageLoaded && _image.hasPalette()) {
			_screen->getPalette().set(_image.getPalette().data(), 0, _image.getPaletteColorCount());
			_screen->applyPalette();
		} else {
			initPalette();
		}
		const Graphics::Surface &surf = _image.getSurface();
		_status = Common::format("%s  %dx%d", ref.toString().c_str(), surf.w, surf.h);
		if (!_imageLoaded)
			_status += "  (could not decode)";
	}

	_status = Common::format("%d/%d  ", index + 1, count) + _status;
	info("Image %s", _status.c_str());
	_dirty = true;
}

void STGEngine::showLevel(const ResourceRef &ref, bool run) {
	std::unique_ptr<Common::SeekableReadStream> stream = _resources.open(ref);
	_hudScreen.reset();
	_heldItem.load(_resources, _levelRef, 0);
	_mission.reset();
	_actors.reset();
	_walker.reset();
	_levelRenderer.reset();
	_level.reset(new LevelFile());
	if (!stream || !_level->load(*stream)) {
		_level.reset();
		initPalette();
		_status = ref.toString() + "  (could not decode)";
		return;
	}
	_levelName = ref.toString();
	_levelRef = ref;
	_punching = false;
	_punchImage.clear();
	_levelTextures = _level->getTextureIds();
	_levelRenderer.reset(new LevelRenderer(*_level));
	_walker.reset(new Walker(*_level));
	loadActors(ref);
	_camera = _levelRenderer->getStartCamera(Walker::kEyeHeight);
	_walker->place(_camera.x, _camera.y, _camera.z - Walker::kEyeHeight);
	startMission(ref, run);
	// The mission's briefing picture, which its opening transition shows.
	{
		size_t slash = ref.name.find_last_of("/\\");
		ResourceRef briefing = { ref.archive, ref.name.substr(0, slash + 1) + "BRIEFING.BMP" };
		_briefingLoaded = false;
		for (const ResourceRef &image : _resources.getImages()) {
			if (image.archive == briefing.archive && Common::toLower(image.name) == Common::toLower(briefing.name))
				_briefingLoaded = _resources.loadImage(image, _briefing) && _briefing.hasPalette();
		}
	}
	_hudScreen.reset();
	_heldItem.load(_resources, _levelRef, 0);
	if (_mission) {
		_hudScreen.reset(new HudScreen(_resources, ref, *_levelScript, _level->getPalette()));
		if (!_hudScreen->isAvailable())
			_hudScreen.reset();
		_levelRenderer->setMapWalls(&_level->getWalls());
	}
	_camera.x = _walker->getX();
	_camera.y = _walker->getY();
	_camera.z = _walker->getEye();
	_levelRenderer->setVisibleZones(_walker->getActiveZones());
	_levelTime = 0;
	_screen->getPalette() = _level->getPalette();
	_screen->applyPalette();
	showLevelPage(0);
}

void STGEngine::loadActors(const ResourceRef &levelRef) {
	// Sprite sets are NNN.gs2 next to GAME.LV2 (0x4C01B0).
	size_t slash = levelRef.name.find_last_of("/\\");
	Common::String folder = slash == Common::String::npos ? "" : levelRef.name.substr(0, slash + 1);
	bool upper = !levelRef.name.empty() && std::isupper((unsigned char)levelRef.name.back());
	auto loader = [this, levelRef, folder, upper](uint16 number, SpriteSet &set) {
		ResourceRef ref = { levelRef.archive, folder + Common::format(upper ? "%03u.GS2" : "%03u.gs2", number) };
		std::unique_ptr<Common::SeekableReadStream> stream = _resources.open(ref);
		if (!stream || !set.load(*stream)) {
			warning("Could not load the sprite set %s", ref.toString().c_str());
			return false;
		}
		return true;
	};
	Walker prototype(*_walker);
	_actors.reset(new Actors(*_level, prototype, loader));
	_walker->setObstacles(&_actors->getObstacles());
	_levelRenderer->setSprites(&_actorSprites);
}

bool STGEngine::findSoundFile(const Common::String &name, const Common::String &archivePrefix, ResourceRef &ref) const {
	// Subtitles sit next to their WAVs, so look for the WAV and swap the extension.
	Common::String base = Common::toLower(name.substr(0, name.rfind('.')));
	Common::String extension = name.substr(name.rfind('.'));
	const ResourceRef *fallback = nullptr;
	for (const ResourceRef &sound : _resources.getSounds()) {
		size_t slash = sound.name.find_last_of("/\\");
		Common::String file = sound.name.substr(slash == Common::String::npos ? 0 : slash + 1);
		if (Common::toLower(file.substr(0, file.rfind('.'))) != base)
			continue;
		if (Common::toLower(sound.archive).compare(0, archivePrefix.size(), Common::toLower(archivePrefix)) == 0) {
			fallback = &sound;
			break;
		}
		if (!fallback)
			fallback = &sound;
	}
	if (!fallback)
		return false;
	ref = *fallback;
	ref.name = ref.name.substr(0, ref.name.rfind('.')) + extension;
	return true;
}

void STGEngine::startMission(const ResourceRef &levelRef, bool run) {
	// A level's scripts are in GAME.BIN next to its GAME.LV2.
	ResourceRef ref = levelRef;
	size_t dot = ref.name.rfind('.');
	if (dot == Common::String::npos)
		return;
	bool upper = dot + 1 < ref.name.size() && std::isupper((unsigned char)ref.name[dot + 1]);
	ref.name = ref.name.substr(0, dot) + (upper ? ".BIN" : ".bin");
	std::unique_ptr<Common::SeekableReadStream> stream = _resources.open(ref);
	if (!stream)
		return;
	_levelScript.reset(new ScriptFile());
	if (!_levelScript->load(*stream)) {
		warning("Could not decode %s; the level will run without its scripts", ref.toString().c_str());
		_levelScript.reset();
		return;
	}
	// Sound files: a mission's own in <mission>/sounds, the rest found by
	// name in COMMON.DAT or the MUSIC archives.
	Common::String folder = levelRef.name.substr(0, levelRef.name.find_last_of("/\\") + 1);
	auto openSound = [this, levelRef, folder](const Common::String &name, MissionSound::Location where)
		-> std::unique_ptr<Common::SeekableReadStream> {
		if (where == MissionSound::kLocationMission) {
			ResourceRef ref = { levelRef.archive, folder + "sounds/" + name };
			if (std::unique_ptr<Common::SeekableReadStream> stream = _resources.open(ref))
				return stream;
		}
		ResourceRef ref;
		if (!findSoundFile(name, where == MissionSound::kLocationMission ? levelRef.archive
		                             : where == MissionSound::kLocationCommon ? "COMMON.DAT" : "MUSIC.",
		                   ref))
			return nullptr;
		return _resources.open(ref);
	};
	_mission.reset(new Mission(*_level, *_levelScript, *_levelRenderer, *_walker, _actors.get(), _mixer, openSound));
	_mission->setDifficulty(_settings.difficulty);
	_mission->setCarriedArray(_carriedArray);
	if (run)
		_mission->start();
	double yaw;
	if (_mission->takePlayerYaw(yaw))
		_camera.yaw = yaw;
	info("Running %s: %d script objects", ref.toString().c_str(), _mission->getVM().getInstanceCount());
}

void STGEngine::showLevelPage(int page) {
	if (!_level)
		return;
	int pages = (int)_levelTextures.size() + kLevelPageTextures;
	_levelPage = ((page % pages) + pages) % pages;
	if (_mission)
		_mission->getSound().pauseAll(_levelPage != kLevelPage3D);
	if (_paletteBlended && _levelPage != kLevelPage3D) {
		_screen->getPalette() = _level->getPalette();
		_screen->applyPalette();
		_paletteBlended = false;
	}
	for (bool &held : _walkKeys)
		held = false;

	if (_levelPage == kLevelPageMap) {
		_status = Common::format("%s  map: %d sectors, %d walls, %d vertices  ([ ] 3D view, textures)",
		                         _levelName.c_str(), (int)_level->getSectors().size(),
		                         (int)_level->getWalls().size(), (int)_level->getVertices().size());
	} else if (_levelPage == kLevelPage3D) {
		updateWalkStatus();
	} else {
		LevelFile::Texture texture;
		int index = _levelPage - kLevelPageTextures;
		uint16 id = _levelTextures[index];
		if (_level->loadTexture(id, texture))
			_status = Common::format("%s  texture %d/%d (id %u) %dx%d", _levelName.c_str(), index + 1,
			                         (int)_levelTextures.size(), id, texture.surface.w, texture.surface.h);
		else
			_status = Common::format("%s  texture %u (could not decode)", _levelName.c_str(), id);
	}
	_dirty = true;
}

bool STGEngine::handleWalkKey(Common::KeyCode key, bool down) {
	int walkKey;
	switch (key) {
	case Common::KEYCODE_UP:
	case Common::KEYCODE_w:
		walkKey = kWalkForward;
		break;
	case Common::KEYCODE_DOWN:
	case Common::KEYCODE_s:
		walkKey = kWalkBack;
		break;
	case Common::KEYCODE_LEFT:
		walkKey = kWalkTurnLeft;
		break;
	case Common::KEYCODE_RIGHT:
		walkKey = kWalkTurnRight;
		break;
	case Common::KEYCODE_a:
		walkKey = kWalkStrafeLeft;
		break;
	case Common::KEYCODE_d:
		walkKey = kWalkStrafeRight;
		break;
	case Common::KEYCODE_PAGEUP:
		walkKey = kWalkLookUp;
		break;
	case Common::KEYCODE_PAGEDOWN:
		walkKey = kWalkLookDown;
		break;
	case Common::KEYCODE_END:
		if (down)
			_camera.pitch = 0;
		return true;
	case Common::KEYCODE_c:
		if (down) {
			_walker->setCollisionEnabled(!_walker->isCollisionEnabled());
			updateWalkStatus();
		}
		return true;
	case Common::KEYCODE_l:
		if (down) {
			_levelRenderer->setLightingEnabled(!_levelRenderer->isLightingEnabled());
			updateWalkStatus();
		}
		return true;
	case Common::KEYCODE_SPACE:
		// Use the open panel's object, as the original's Space does.
		if (down && _mission)
			_mission->getHud().usePanel();
		return true;
	case Common::KEYCODE_b:
	case Common::KEYCODE_h:
	case Common::KEYCODE_o:
		// The left panel: life, map, object (the original's B, H, A; A walks here).
		if (down && _hudScreen)
			_hudScreen->setLeftMode(key == Common::KEYCODE_b   ? HudScreen::kLeftLife
			                        : key == Common::KEYCODE_h ? HudScreen::kLeftMap
			                                                   : HudScreen::kLeftObject);
		return true;
	case Common::KEYCODE_LCTRL:
	case Common::KEYCODE_RCTRL:
		// The original's default fire key; one attack per press.
		if (down && !_fireHeld)
			attack();
		_fireHeld = down;
		return true;
	case Common::KEYCODE_1:
	case Common::KEYCODE_2:
	case Common::KEYCODE_3:
	case Common::KEYCODE_4:
	case Common::KEYCODE_5:
	case Common::KEYCODE_6:
	case Common::KEYCODE_7:
	case Common::KEYCODE_8:
	case Common::KEYCODE_9:
		// Until there is an inventory panel: use the item in that slot.
		if (down && _mission) {
			_mission->useItem(key - Common::KEYCODE_1);
			updateWalkStatus();
		}
		return true;
	default:
		return false;
	}
	_walkKeys[walkKey] = down;
	return true;
}

void STGEngine::updateWalk(uint32 delta) {
	const double kSpeed = 25.0;    // world units per second
	const double kTurnSpeed = 2.0; // radians per second
	const double kLookSpeed = 0.8; // pitch per second
	double seconds = std::min<uint32>(delta, 100) / 1000.0;

	double forward = (_walkKeys[kWalkForward] ? 1 : 0) - (_walkKeys[kWalkBack] ? 1 : 0);
	double strafe = (_walkKeys[kWalkStrafeRight] ? 1 : 0) - (_walkKeys[kWalkStrafeLeft] ? 1 : 0);
	double turn = (_walkKeys[kWalkTurnLeft] ? 1 : 0) - (_walkKeys[kWalkTurnRight] ? 1 : 0);
	double look = (_walkKeys[kWalkLookDown] ? 1 : 0) - (_walkKeys[kWalkLookUp] ? 1 : 0);
	// No moving while a transition holds the screen, nor without the
	// controls the scripts allow (SET_PLAYER_CONTROLS: 1 walk, 2 turn,
	// 4 look; the rest gate jumping, running and swimming).
	if (_mission && _mission->getTransitionView().active)
		forward = strafe = turn = look = 0;
	if (_mission) {
		uint16 controls = _mission->getControls();
		if (!(controls & 1))
			forward = strafe = 0;
		if (!(controls & 2))
			turn = 0;
		if (!(controls & 4))
			look = 0;
	}
	// The scripts' motion settings (SET_PLAYER_MOTION) scale the viewer's
	// speeds, the original's defaults giving these; cutscenes set 0.
	double moveScale = 1, turnScale = 1, lookScale = 1;
	if (_mission) {
		static const Mission::Motion kDefault;
		const Mission::Motion &motion = _mission->getMotion();
		moveScale = motion.speed / kDefault.speed;
		turnScale = (double)motion.turnRate / kDefault.turnRate;
		lookScale = (double)motion.lookRate / kDefault.lookRate;
	}

	_camera.yaw += turn * kTurnSpeed * turnScale * seconds;
	// SET_PLAYER_SPIN: turned regardless, degrees a second (heading up is yaw down).
	if (_mission && _mission->getSpin())
		_camera.yaw -= _mission->getSpin() * kPi / 180 * seconds;
	_camera.pitch = std::max(-1.0, std::min(1.0, _camera.pitch + look * kLookSpeed * lookScale * seconds));
	double step = kSpeed * moveScale * seconds;
	double dx = (std::cos(_camera.yaw) * forward + std::sin(_camera.yaw) * strafe) * step;
	double dy = (std::sin(_camera.yaw) * forward - std::cos(_camera.yaw) * strafe) * step;

	// Move every frame, so falls carry on without a key held.
	double oldX = _camera.x, oldY = _camera.y, oldZ = _camera.z;
	_walker->move(dx, dy, seconds);
	if (_mission) {
		_mission->setPlayerYaw(_camera.yaw);
		_mission->update(delta);
		double yaw;
		if (_mission->takePlayerYaw(yaw))
			_camera.yaw = yaw;
		Common::String subtitle;
		uint32 duration;
		if (_mission->takeSubtitle(subtitle, duration) && (_settings.subtitles || !_frontEnd)) {
			_subtitle = subtitle;
			_missionSubtitle = true;
			_subtitleUntil = _levelTime + duration;
			info("%s", subtitle.c_str());
		}
	} else if (_actors) {
		std::vector<Actors::ScriptEvent> events;
		_actors->update(delta, *_walker, events);
	}
	_camera.x = _walker->getX();
	_camera.y = _walker->getY();
	_camera.z = _walker->getEye();
	_levelRenderer->setVisibleZones(_walker->getActiveZones());
	if (turn != 0 || look != 0 || _camera.x != oldX || _camera.y != oldY || _camera.z != oldZ)
		updateWalkStatus();
}

void STGEngine::prepareAim(int w, int h) {
	// The original aims at the cursor while it is over the view, else at
	// the centre (view.vtD8 0x430756).
	_viewW = w;
	_viewH = h;
	bool inView = _mouseX >= 0 && _mouseX < w && _mouseY >= 0 && _mouseY < h;
	_aimX = inView ? _mouseX : w / 2;
	_aimY = inView ? _mouseY : h / 2;

	int kind = 0;
	if (_mission && _mission->getActiveItem() && _mission->isWeapon(_mission->getActiveItem()))
		kind = _mission->getItemKind(_mission->getActiveItem());
	if (kind != _heldItem.getKind())
		_heldItem.load(_resources, _levelRef, kind);

	// Weapons act on what the cursor points at, the punch on the centre.
	if (kind)
		_levelRenderer->setPickPoint(_aimX, _aimY);
	else
		_levelRenderer->setPickPoint(w / 2, h / 2);
	setCursor(kind && inView && HeldItem::getStyle(kind) &&
	          HeldItem::place(*HeldItem::getStyle(kind), _aimX, _aimY, w, h).usable);
}

void STGEngine::drawViewOverlay(Graphics::Surface &view) {
	// SHOW_VIEW_OVERLAY: centred at the top of the view, like the punch.
	const Mission::ViewOverlay &overlay = _mission->getViewOverlay();
	if (overlay.image != _viewOverlayImage) {
		_viewOverlayImage = overlay.image;
		_viewOverlayLoaded = false;
		if (!overlay.image.empty()) {
			size_t slash = _levelRef.name.find_last_of("/\\");
			Common::String folder = slash == Common::String::npos ? "" : _levelRef.name.substr(0, slash + 1);
			std::unique_ptr<Common::SeekableReadStream> stream =
				_resources.open({ _levelRef.archive, folder + overlay.image });
			_viewOverlayLoaded = stream && _viewOverlay.load(*stream);
			if (!_viewOverlayLoaded)
				warning("Could not load the view overlay %s", overlay.image.c_str());
		}
	}
	if (!_viewOverlayLoaded)
		return;
	int height = _viewOverlay.guessFrameHeight();
	_viewOverlay.drawFrame(view, overlay.frame, height, (view.w - _viewOverlay.getWidth()) / 2, 0);
}

void STGEngine::drawHands(Graphics::Surface &view, uint32 delta) {
	if (_mission)
		drawViewOverlay(view);
	if (_heldItem.getKind()) {
		_heldItem.draw(view, _aimX, _aimY, _flashLeft > 0);
		_flashLeft = _flashLeft > delta ? _flashLeft - delta : 0;
		return;
	}
	updatePunch(delta, view);
	byte cross = _screen->getPalette().findBestColor(255, 255, 255);
	for (int d = 2; d <= 5; d++) {
		view.setPixel(view.w / 2 - d, view.h / 2, cross);
		view.setPixel(view.w / 2 + d, view.h / 2, cross);
		view.setPixel(view.w / 2, view.h / 2 - d, cross);
		view.setPixel(view.w / 2, view.h / 2 + d, cross);
	}
}

void STGEngine::attack() {
	if (!_mission || _punching)
		return;
	if (_heldItem.getKind()) {
		// From the gun's muzzle, which follows the cursor; below the gun's
		// line it can't be used.
		double ahead = 1.5, up = -0.5, left = 0;
		if (const HeldItem::Style *style = HeldItem::getStyle(_heldItem.getKind())) {
			if (!HeldItem::place(*style, _aimX, _aimY, _viewW, _viewH).usable)
				return;
			HeldItem::getMuzzle(*style, _aimX, _aimY, _viewW, _viewH, ahead, up, left);
		}
		Mission::FireResult result = _mission->fireWeapon(_camera, _levelRenderer->getPick(), _aimX, _aimY, _viewW,
		                                                  _viewH, ahead, up, left);
		if (result == Mission::kFired) {
			const Mission::Item *item = _mission->getItem(_mission->getActiveItem());
			_flashLeft = item && item->setting > 0 ? item->setting : 150;
			updateWalkStatus();
		}
		if (result != Mission::kNoWeapon)
			return;
	}

	// Bare hands: play the punch overlay (SET_PUNCH), which lands on its
	// hit frame. It sits beside GAME.LV2.
	const Mission::Punch &punch = _mission->getPunch();
	if (punch.image != _punchImage) {
		_punchImage = punch.image;
		_punchFrameHeight = 0;
		size_t slash = _levelRef.name.find_last_of("/\\");
		Common::String folder = slash == Common::String::npos ? "" : _levelRef.name.substr(0, slash + 1);
		std::unique_ptr<Common::SeekableReadStream> stream = _resources.open({ _levelRef.archive, folder + punch.image });
		if (stream && _punchOverlay.load(*stream))
			_punchFrameHeight = _punchOverlay.guessFrameHeight();
		else if (!punch.image.empty())
			warning("Could not load the punch overlay %s", punch.image.c_str());
	}
	_mission->swing();
	_punching = true;
	_punchLanded = false;
	_punchFrame = 0;
	_punchTime = 0;
}

void STGEngine::updatePunch(uint32 delta, Graphics::Surface &view) {
	if (!_punching || !_mission)
		return;
	const Mission::Punch &punch = _mission->getPunch();
	int frames = _punchFrameHeight ? _punchOverlay.getRowCount() / _punchFrameHeight : 0;
	// One frame per tick at most (0x431E57); the blow lands on the first
	// frame showing the hit frame.
	_punchTime += delta;
	_punchFrame = std::min<int>(_punchTime / punch.msPerFrame, _punchFrame + 1);
	if (!_punchLanded && (_punchFrame >= punch.hitFrame || frames == 0)) {
		_punchLanded = true;
		_mission->punch(_levelRenderer->getPick());
	}
	if (_punchFrame >= frames) {
		_punching = false;
		return;
	}
	_punchOverlay.drawFrame(view, _punchFrame, _punchFrameHeight, (view.w - _punchOverlay.getWidth()) / 2,
	                        view.h - _punchFrameHeight);
}

void STGEngine::updateWalkStatus() {
	double degrees = std::fmod(_camera.yaw * 180 / 3.14159265358979323846, 360.0);
	if (degrees < 0)
		degrees += 360;
	// The archive names the level, e.g. ARVADA1.DAT:arvada1/GAME.LV2.
	Common::String level = _levelName.substr(0, _levelName.find(':'));
	if (Common::hasSuffixIgnoreCase(level, ".dat"))
		level = level.substr(0, level.size() - 4);
	_status = Common::format("%s 3D %.0f,%.0f,%.0f %.0fdeg  WASD PgUp/Dn  L: light %s  C: walls %s", level.c_str(),
	                         _camera.x, _camera.y, _camera.z, degrees,
	                         _levelRenderer->isLightingEnabled() ? "on" : "off",
	                         _walker->isCollisionEnabled() ? "on" : "off");
	if (_mission) {
		_shownHealth = _mission->getHealth();
		_status += Common::format("  health %d", _shownHealth);
		if (_mission->getActiveItem())
			_status += Common::format("  item %04x", _mission->getActiveItem());
		const Mission::Item *item = _mission->getItem(_mission->getActiveItem());
		if (item && item->maxLevel > 0)
			_status += Common::format(" power %d charge %d/%d", item->level, item->charge, item->maxCharge);
	}
}

void STGEngine::showSprites(const ResourceRef &ref) {
	std::unique_ptr<Common::SeekableReadStream> stream = _resources.open(ref);
	_spritesLoaded = stream && _sprites.load(*stream) && _sprites.getFrameCount() > 0;
	if (!_spritesLoaded) {
		initPalette();
		_status = ref.toString() + "  (could not decode)";
		return;
	}

	// Sprites carry no palette of their own: they are drawn with the 3D
	// level's, from the GAME.LV2 beside them. Failing that, borrow one
	// from an image in the same archive.
	Graphics::Palette palette;
	size_t slash = ref.name.find_last_of("/\\");
	Common::String folder = slash == Common::String::npos ? "" : ref.name.substr(0, slash + 1);
	std::unique_ptr<Common::SeekableReadStream> levelStream = _resources.open({ ref.archive, folder + "GAME.LV2" });
	LevelFile level;
	if (levelStream && level.load(*levelStream)) {
		_screen->getPalette() = level.getPalette();
		_screen->applyPalette();
	} else if (_resources.loadArchivePalette(ref.archive, palette)) {
		_screen->getPalette() = palette;
		_screen->applyPalette();
	} else {
		initPalette();
	}

	int sequences = 0;
	for (const std::vector<byte> &sequence : _sprites.getSequences())
		sequences += !sequence.empty();
	_overlayFrame = 0;
	_overlayTime = 0;
	_status = Common::format("%s  %d frames up to %dx%d, %d sequences", ref.toString().c_str(),
	                         _sprites.getFrameCount(), _sprites.getMaxWidth(), _sprites.getMaxHeight(), sequences);
}

void STGEngine::showOverlay(const ResourceRef &ref) {
	std::unique_ptr<Common::SeekableReadStream> stream = _resources.open(ref);
	_overlayLoaded = stream && _overlay.load(*stream);
	if (!_overlayLoaded) {
		initPalette();
		_status = ref.toString() + "  (could not decode)";
		return;
	}

	// Overlays carry no palette; borrow one from an image in the same archive.
	Graphics::Palette palette;
	if (_resources.loadArchivePalette(ref.archive, palette)) {
		_screen->getPalette() = palette;
		_screen->applyPalette();
	} else {
		initPalette();
	}

	_overlayFrameHeight = _overlay.guessFrameHeight();
	_overlayFrame = 0;
	_overlayTime = 0;
	_status = Common::format("%s  %dx%d, %d frames (frame height guessed)", ref.toString().c_str(), _overlay.getWidth(),
	                         _overlayFrameHeight, _overlay.getRowCount() / _overlayFrameHeight);
}

void STGEngine::jumpArchive(int direction) {
	const std::vector<ResourceRef> &images = _resources.getImages();
	if (images.empty())
		return;

	// Index of the first image of each archive.
	std::vector<int> starts;
	for (int i = 0; i < (int)images.size(); i++) {
		if (i == 0 || images[i].archive != images[i - 1].archive)
			starts.push_back(i);
	}

	int current = std::max(_imageIndex, 0);
	int group = 0;
	while (group + 1 < (int)starts.size() && starts[group + 1] <= current)
		group++;
	int n = (int)starts.size();
	showImage(starts[((group + direction) % n + n) % n]);
}

void STGEngine::jumpLevel(int direction) {
	const std::vector<ResourceRef> &images = _resources.getImages();
	int count = (int)images.size();
	for (int step = 1; step <= count; step++) {
		int index = (((_imageIndex + direction * step) % count) + count) % count;
		if (Common::hasSuffixIgnoreCase(images[index].name, ".lv2")) {
			showImage(index);
			return;
		}
	}
}

void STGEngine::showStarfield() {
	_imageLoaded = false;
	_overlayLoaded = false;
	_spritesLoaded = false;
	_hudScreen.reset();
	_heldItem.load(_resources, _levelRef, 0);
	_mission.reset();
	_actors.reset();
	_walker.reset();
	_levelRenderer.reset();
	_level.reset();
	_imageIndex = -1;
	initPalette();
	_status = "Up/Down, Enter: missions   Left/Right: images   PgUp/PgDn: archives   Tab: levels   Esc: quit";
	_dirty = true;
}

void STGEngine::playSound(int index) {
	const std::vector<ResourceRef> &sounds = _resources.getSounds();
	if (sounds.empty()) {
		playBeep(440);
		return;
	}

	int count = (int)sounds.size();
	index = ((index % count) + count) % count;
	_soundIndex = index;
	const ResourceRef &ref = sounds[index];

	std::unique_ptr<Audio::RewindableAudioStream> stream = _resources.loadSound(ref);
	if (!stream)
		return;

	bool music = Common::matchString(ref.archive, "MUSIC.*");
	_subtitle = music ? Common::String() : _resources.loadSubtitle(ref);
	Common::String desc = Common::format("Sound %d/%d: %s", index + 1, count, ref.toString().c_str());
	info("%s%s%s", desc.c_str(), _subtitle.empty() ? "" : " - ", _subtitle.c_str());
	if (_subtitle.empty())
		_subtitle = desc;

	_mixer->stopHandle(_soundHandle);
	_mixer->playStream(music ? Audio::Mixer::kMusicSoundType : Audio::Mixer::kSpeechSoundType, &_soundHandle,
	                   std::move(stream));
	_dirty = true;
}

void STGEngine::playBeep(int frequency) {
	// 150 ms sine with a short linear fade-out, generated at 22050 Hz.
	const int rate = 22050;
	const int length = rate * 150 / 1000;
	std::vector<byte> data(length * 2);
	for (int i = 0; i < length; i++) {
		double env = i > length * 3 / 4 ? (double)(length - i) / (length / 4) : 1.0;
		int16 sample = (int16)(sin(2.0 * kPi * frequency * i / rate) * 12000 * env);
		WRITE_LE_UINT16(&data[i * 2], (uint16)sample);
	}
	_mixer->playStream(Audio::Mixer::kSFXSoundType, nullptr,
	                   Audio::makeRawStream(std::move(data), rate, Audio::FLAG_16BITS | Audio::FLAG_LITTLE_ENDIAN));
}

bool STGEngine::playMovie(const Common::String &name) {
	Movie movie;
	if (!movie.load(_resources.open({ "", name }))) {
		warning("Can't play the movie %s", name.c_str());
		return false;
	}

	// The movies are true colour: the screen leaves 8-bit mode while they
	// play, letterboxed in the middle of it.
	const Graphics::PixelFormat format = Graphics::PixelFormat::createFormatARGB8888();
	_system->initSize(Screen::kWidth, Screen::kHeight, &format);
	_system->showMouse(false);

	Audio::SoundHandle sound;
	if (!movie.getAudio().empty() && movie.getAudioBits() == 16) {
		std::vector<byte> data = movie.getAudio();
		byte flags = Audio::FLAG_16BITS | Audio::FLAG_LITTLE_ENDIAN;
		if (movie.getAudioChannels() == 2)
			flags |= Audio::FLAG_STEREO;
		_mixer->playStream(Audio::Mixer::kMusicSoundType, &sound,
		                   Audio::makeRawStream(std::move(data), movie.getAudioRate(), flags));
	}

	int x = (Screen::kWidth - std::min(movie.getWidth(), (int)Screen::kWidth)) / 2;
	int y = (Screen::kHeight - std::min(movie.getHeight(), (int)Screen::kHeight)) / 2;
	int w = std::min(movie.getWidth(), (int)Screen::kWidth), h = std::min(movie.getHeight(), (int)Screen::kHeight);
	uint32 start = _system->getMillis();
	bool done = false;
	while (!done && !shouldQuit()) {
		Common::Event event;
		while (_eventMan->pollEvent(event)) {
			if (event.type == Common::EVENT_LBUTTONDOWN ||
			    (event.type == Common::EVENT_KEYDOWN &&
			     (event.kbd.keycode == Common::KEYCODE_ESCAPE || event.kbd.keycode == Common::KEYCODE_SPACE ||
			      event.kbd.keycode == Common::KEYCODE_RETURN)))
				done = true;
		}

		// Keep to the clock, dropping frames if decoding falls behind.
		int due = (int)((uint64)(_system->getMillis() - start) * 1000 / movie.getFrameTime());
		bool drawn = false;
		while (!done && movie.getCurrentFrame() < due) {
			if (!movie.nextFrame())
				done = true;
			drawn = true;
		}
		if (drawn && movie.getCurrentFrame() >= 0) {
			_system->copyRectToScreen(movie.getPicture().data(), movie.getWidth() * 4, x, y, w, h);
			_system->updateScreen();
		}
		_system->delayMillis(5);
	}

	_mixer->stopHandle(sound);
	_system->initSize(Screen::kWidth, Screen::kHeight);
	_screen->applyPalette();
	_system->showMouse(true);
	return true;
}

Common::String STGEngine::getSaveFileName(int slot) const {
	Common::String target = ConfMan.getActiveDomain();
	if (target.empty())
		target = _game.gameId;
	return Common::format("%s.s%02d", target.c_str(), slot);
}

Common::Error STGEngine::saveGameState(int slot, const Common::String &description) {
	std::unique_ptr<Common::OutSaveFile> out = _saveFileMan->openForSaving(getSaveFileName(slot));
	if (!out)
		return Common::kWritingFailed;

	out->writeUint32BE(kSaveMagic);
	out->writeUint32LE(kSaveVersion);
	out->writeUint16LE((uint16)description.size());
	out->writeString(description);
	Common::String date = saveDate();
	out->writeUint16LE((uint16)date.size());
	out->writeString(date);
	out->writeUint32LE((uint32)_imageIndex);
	out->writeUint32LE((uint32)_soundIndex);
	// Where the story is.
	out->writeByte(_frontEnd ? 1 : 0);
	if (_frontEnd) {
		Common::Serializer s(nullptr, out.get());
		_campaign.sync(s);
		bool bridge = _bridge != nullptr;
		s.syncAsBool(bridge);
		if (bridge)
			_bridge->sync(s);
	}
	// A mission in progress: where, the view, then everything the mission holds.
	bool inMission = _playingMission && _mission && _level;
	out->writeByte(inMission ? 1 : 0);
	if (inMission) {
		Common::Serializer s(nullptr, out.get());
		Common::String name = _missions[_missionChoice].name;
		s.syncString(name);
		s.syncAsDouble(_camera.yaw);
		s.syncAsDouble(_camera.pitch);
		int leftMode = _hudScreen ? (int)_hudScreen->getLeftMode() : 0;
		s.syncAsInt(leftMode);
		_mission->sync(s);
		if (s.err())
			return Common::kWritingFailed;
	}
	if (!out->flush() || out->err())
		return Common::kWritingFailed;

	info("Saved slot %d to %s", slot, getSaveFileName(slot).c_str());
	return Common::kNoError;
}

Common::Error STGEngine::loadGameState(int slot) {
	std::unique_ptr<Common::InSaveFile> in = _saveFileMan->openForLoading(getSaveFileName(slot));
	if (!in)
		return Common::Error(Common::kReadingFailed, "no saved game in slot " + std::to_string(slot));

	if (in->readUint32BE() != kSaveMagic)
		return Common::Error(Common::kReadingFailed, "not a saved game");
	uint32 version = in->readUint32LE();
	if (version > kSaveVersion)
		return Common::Error(Common::kReadingFailed, "saved by a newer version");

	Common::String description = in->readFixedString(in->readUint16LE());
	if (version >= kFirstDatedSaveVersion)
		in->readFixedString(in->readUint16LE());
	int32 imageIndex = in->readSint32LE();
	int32 soundIndex = in->readSint32LE();
	bool inCampaign = false;
	if (version >= kFirstDatedSaveVersion && in->readByte() != 0) {
		if (!_frontEnd)
			return Common::Error(Common::kReadingFailed, "a saved game, which the resource viewer can't load");
		if (version < kFirstBridgeSaveVersion)
			return Common::Error(Common::kReadingFailed, "saved by an older build, before the bridge; start a new game");
		Campaign campaign = _campaign;
		Common::Serializer s(in.get(), nullptr);
		_campaign.sync(s);
		bool bridge = false;
		s.syncAsBool(bridge);
		if (bridge && _bridge) {
			_bridge->reset();
			_bridge->sync(s);
		} else if (bridge) {
			s.fail();
		}
		if (s.err()) {
			_campaign = campaign;
			return Common::Error(Common::kReadingFailed, "damaged saved game");
		}
		inCampaign = true;
	}
	bool inMission = version >= 2 && in->readByte() != 0;
	if (in->eos())
		return Common::Error(Common::kReadingFailed, "truncated saved game");

	if (inMission && version < kFirstMissionSaveVersion)
		return Common::Error(Common::kReadingFailed, "the mission was saved by an older build; start it again");
	if (inMission) {
		Common::Serializer s(in.get(), nullptr);
		Common::String name;
		s.syncString(name);
		int index = -1;
		for (int i = 0; i < (int)_missions.size(); i++) {
			if (_missions[i].name == name)
				index = i;
		}
		if (index < 0)
			return Common::Error(Common::kReadingFailed, "the mission " + std::string(name.c_str()) + " isn't installed");
		double yaw = 0, pitch = 0;
		int leftMode = 0;
		s.syncAsDouble(yaw);
		s.syncAsDouble(pitch);
		s.syncAsInt(leftMode);
		// Set the level up without starting its scripts, then put the
		// saved state over it.
		_missionChoice = index;
		_missionMessage.clear();
		showLevel(_missions[index].level, false);
		if (!_level || !_mission)
			return Common::Error(Common::kReadingFailed, "could not load the mission");
		_mission->sync(s);
		if (s.err()) {
			leaveMission("The saved game could not be read");
			return Common::Error(Common::kReadingFailed, "damaged saved game");
		}
		_camera.yaw = yaw;
		_camera.pitch = pitch;
		_camera.x = _walker->getX();
		_camera.y = _walker->getY();
		_camera.z = _walker->getEye();
		_levelRenderer->setVisibleZones(_walker->getActiveZones());
		if (_hudScreen && leftMode >= HudScreen::kLeftOff && leftMode <= HudScreen::kLeftObject)
			_hudScreen->setLeftMode((HudScreen::LeftMode)leftMode);
		_playingMission = true;
		showLevelPage(kLevelPage3D);
		info("Loaded slot %d: %s (%s)", slot, description.c_str(), name.c_str());
		return Common::kNoError;
	}

	if (inCampaign) {
		// Saved between missions: the story carries on from there, the
		// section starting again from its first line (0x419703).
		if (_playingMission)
			leaveMission("Loaded a saved game");
		if (_bridge)
			_bridge->afterLoad();
		_campaign.restartSection();
		info("Loaded slot %d: %s", slot, description.c_str());
		runCampaign();
		return Common::kNoError;
	}

	_soundIndex = soundIndex;
	if (imageIndex >= 0)
		showImage(imageIndex);

	info("Loaded slot %d: %s", slot, description.c_str());
	return Common::kNoError;
}

} // End of namespace STG
