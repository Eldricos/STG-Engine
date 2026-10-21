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

	_resources.scan();
	_resources.logInventory();

	if (_game.unknownVariant)
		warning("This version of %s is not known; only the resource viewer is available", _game.description.c_str());

	info("Controls: Left/Right = browse images, Space = play next sound, click = test tone,");
	info("          F5 = save, F7 = load, Alt+Enter = fullscreen, Esc = quit");

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
		case Common::KEYCODE_SPACE:
			playSound(_soundIndex + 1);
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
	if (_imageLoaded) {
		// Static image: only redraw when something changed.
		if (!_dirty) {
			_system->updateScreen();
			return;
		}
		_screen->clear(kColorBlack);
		if (_image.hasPalette()) {
			_screen->getPalette().set(_image.getPalette().data(), 0, _image.getPaletteColorCount());
			_screen->applyPalette();
		}
		_screen->drawCentered(_image.getSurface());
	} else {
		_screen->clear(kColorBlack);
		drawStarfield(delta);
		Graphics::Surface &s = _screen->getSurface();
		s.frameRect(Common::Rect(8, 8, Screen::kWidth - 8, Screen::kHeight - 8), kColorFrame);
	}

	_screen->update();
	_dirty = false;
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
	const Common::StringArray &images = _resources.getImages();
	if (images.empty())
		return;

	int count = (int)images.size();
	index = ((index % count) + count) % count;
	_imageIndex = index;
	_imageLoaded = _resources.loadImage(images[index], _image);
	if (!_imageLoaded) {
		// Back to the test pattern, which needs its own palette.
		initPalette();
	} else if (!_image.hasPalette()) {
		initPalette();
	}
	info("Image %d/%d: %s (%dx%d %s)", index + 1, count, images[index].c_str(), _image.getSurface().w,
	     _image.getSurface().h, _image.getSurface().format.toString().c_str());
	_dirty = true;
}

void STGEngine::playSound(int index) {
	const Common::StringArray &sounds = _resources.getSounds();
	if (sounds.empty()) {
		playBeep(440);
		return;
	}

	int count = (int)sounds.size();
	index = ((index % count) + count) % count;
	_soundIndex = index;

	std::unique_ptr<Audio::RewindableAudioStream> stream = _resources.loadSound(sounds[index]);
	if (!stream)
		return;
	info("Sound %d/%d: %s (%d Hz, %s)", index + 1, count, sounds[index].c_str(), stream->getRate(),
	     stream->isStereo() ? "stereo" : "mono");
	_mixer->stopHandle(_soundHandle);
	_mixer->playStream(Audio::Mixer::kSFXSoundType, &_soundHandle, std::move(stream));
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
