/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/stg.h"

#include <cmath>

#include "common/audio/raw.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/savefile.h"
#include "common/system.h"

namespace STG {

namespace {

const uint32 kSaveMagic = MKTAG('S', 'T', 'G', 'S');
const uint32 kSaveVersion = 1;
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

	if (_game.unknownVariant)
		warning("This version of %s is not known; only the resource viewer is available", _game.description.c_str());

	info("%d images and %d sounds found", (int)_resources.getImages().size(), (int)_resources.getSounds().size());
	info("Controls: Left/Right = image, PgUp/PgDn = archive, Up/Down = sound, Space = replay sound,");
	info("          Home = starfield, click = test tone, F5/F7 = save/load, Alt+Enter = fullscreen, Esc = quit");
	_status = "Left/Right: images   PgUp/PgDn: archives   Up/Down: sounds   Esc: quit";

	for (int i = 0; i < kNumStars; i++)
		_stars.push_back({ (int32)_random.getRandomNumberRng(0, 2000) - 1000,
		                   (int32)_random.getRandomNumberRng(0, 2000) - 1000,
		                   (int32)_random.getRandomNumberRng(1, 1000) });

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
	// A 15x15 crosshair; index 0 is transparent.
	const int size = 15;
	byte cursor[size * size] = {};
	for (int i = 0; i < size; i++) {
		if (i == size / 2)
			continue;
		cursor[(size / 2) * size + i] = kColorWhite;
		cursor[i * size + size / 2] = kColorWhite;
	}
	_system->setMouseCursor(cursor, size, size, size / 2, size / 2, 0);
	_system->showMouse(true);
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
	switch (event.type) {
	case Common::EVENT_KEYDOWN:
		switch (event.kbd.keycode) {
		case Common::KEYCODE_ESCAPE:
			quitGame();
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
		case Common::KEYCODE_DOWN:
			playSound(_soundIndex + 1);
			break;
		case Common::KEYCODE_UP:
			playSound(_soundIndex - 1);
			break;
		case Common::KEYCODE_SPACE:
			playSound(std::max(_soundIndex, 0));
			break;
		case Common::KEYCODE_HOME:
			showStarfield();
			break;
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
	case Common::EVENT_LBUTTONDOWN:
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
	if (!_subtitle.empty() && !_mixer->isSoundHandleActive(_soundHandle)) {
		_subtitle.clear();
		_dirty = true;
	}

	if (_level) {
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
			if (_level->loadTexture(_levelTextures[_levelPage - 1], texture))
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
	}

	drawTextPanel();
	_screen->update();
	_dirty = false;
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

void STGEngine::showLevel(const ResourceRef &ref) {
	std::unique_ptr<Common::SeekableReadStream> stream = _resources.open(ref);
	_level.reset(new LevelFile());
	if (!stream || !_level->load(*stream)) {
		_level.reset();
		initPalette();
		_status = ref.toString() + "  (could not decode)";
		return;
	}
	_levelName = ref.toString();
	_levelTextures = _level->getTextureIds();
	_screen->getPalette() = _level->getPalette();
	_screen->applyPalette();
	showLevelPage(0);
}

void STGEngine::showLevelPage(int page) {
	if (!_level)
		return;
	int pages = (int)_levelTextures.size() + 1;
	_levelPage = ((page % pages) + pages) % pages;

	if (_levelPage == 0) {
		_status = Common::format("%s  map: %d sectors, %d walls, %d vertices  ([ ] textures)", _levelName.c_str(),
		                         (int)_level->getSectors().size(), (int)_level->getWalls().size(),
		                         (int)_level->getVertices().size());
	} else {
		LevelFile::Texture texture;
		uint16 id = _levelTextures[_levelPage - 1];
		if (_level->loadTexture(id, texture))
			_status = Common::format("%s  texture %d/%d (id %u) %dx%d", _levelName.c_str(), _levelPage,
			                         (int)_levelTextures.size(), id, texture.surface.w, texture.surface.h);
		else
			_status = Common::format("%s  texture %u (could not decode)", _levelName.c_str(), id);
	}
	_dirty = true;
}

void STGEngine::showSprites(const ResourceRef &ref) {
	std::unique_ptr<Common::SeekableReadStream> stream = _resources.open(ref);
	_spritesLoaded = stream && _sprites.load(*stream) && _sprites.getFrameCount() > 0;
	if (!_spritesLoaded) {
		initPalette();
		_status = ref.toString() + "  (could not decode)";
		return;
	}

	// Like overlays, sprites carry no palette of their own.
	Graphics::Palette palette;
	if (_resources.loadArchivePalette(ref.archive, palette)) {
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

void STGEngine::showStarfield() {
	_imageLoaded = false;
	_overlayLoaded = false;
	_spritesLoaded = false;
	_level.reset();
	_imageIndex = -1;
	initPalette();
	_status = "Left/Right: images   PgUp/PgDn: archives   Up/Down: sounds   Esc: quit";
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
	out->writeUint32LE((uint32)_imageIndex);
	out->writeUint32LE((uint32)_soundIndex);
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
	int32 imageIndex = in->readSint32LE();
	int32 soundIndex = in->readSint32LE();
	if (in->eos())
		return Common::Error(Common::kReadingFailed, "truncated saved game");

	_soundIndex = soundIndex;
	if (imageIndex >= 0)
		showImage(imageIndex);

	info("Loaded slot %d: %s", slot, description.c_str());
	return Common::kNoError;
}

} // End of namespace STG
