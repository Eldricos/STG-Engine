/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "test/test.h"

#include <cstring>
#include <vector>

#include "common/audio/mixer.h"
#include "common/endian.h"
#include "common/stream.h"
#include "engines/stg/sound.h"

namespace {

using STG::MissionSound;

/** A 56-byte sound table entry. */
std::vector<byte> soundEntry(uint32 key, uint32 flags, int positionMode = 0, double x = 0, double radius = 0,
                             int32 repeats = 0) {
	std::vector<byte> r(56);
	WRITE_LE_UINT32(&r[0x00], key);
	WRITE_LE_UINT32(&r[0x04], 64);
	WRITE_LE_UINT32(&r[0x08], 1);
	WRITE_LE_UINT32(&r[0x0C], positionMode);
	WRITE_LE_UINT32(&r[0x14], (uint32)(int32)(x * 65536));
	WRITE_LE_UINT32(&r[0x20], (uint32)(int32)(radius * 65536));
	WRITE_LE_UINT32(&r[0x28], (uint32)repeats);
	WRITE_LE_UINT32(&r[0x34], flags);
	return r;
}

/**
 * A script with no objects and this sound table: 0 Riker speaking, 1 a
 * sound at (10, 0, 0) heard within 20 units, 2 a COMMON sound, 3 music;
 * and cue 5, which plays sound 0, waits 500 ms and plays it again.
 */
std::vector<byte> makeSoundScript() {
	std::vector<std::vector<byte>> sections(6);
	for (const std::vector<byte> &e :
	     { soundEntry(0x4006B, MissionSound::kFlagSubtitle), soundEntry(0x00003, 0, 1, 10, 20),
	       soundEntry(0x1000032, 0), soundEntry(0x121D4D, MissionSound::kFlagStream) })
		sections[3].insert(sections[3].end(), e.begin(), e.end());
	std::vector<byte> end(56, 0xFF);
	sections[3].insert(sections[3].end(), end.begin(), end.end());

	const uint32 cue[] = { 0x12345678, 5, 0x20, 0, 0, 0, (uint32)-500, 0, 0, 0, 0x87654321, 0xFFFFFFFF };
	for (uint32 word : cue) {
		byte b[4];
		WRITE_LE_UINT32(b, word);
		sections[2].insert(sections[2].end(), b, b + 4);
	}

	std::vector<byte> file(40);
	WRITE_BE_UINT32(&file[0], MKTAG('S', 'G', 'E', 'N'));
	WRITE_LE_UINT16(&file[4], 22);
	WRITE_LE_UINT16(&file[6], 2);
	WRITE_LE_UINT32(&file[12], 2);
	for (int i = 0; i < 6; i++)
		WRITE_LE_UINT32(&file[16 + i * 4], (uint32)sections[i].size());
	file.push_back(0x00);
	file.push_back(0x10); // the program: end marker 0x1000
	for (const std::vector<byte> &s : sections)
		file.insert(file.end(), s.begin(), s.end());
	WRITE_LE_UINT32(&file[8], (uint32)file.size() - 12);
	return file;
}

/** 100 ms of 22 kHz mono silence. */
std::vector<byte> makeWAV() {
	const uint32 samples = 2205;
	std::vector<byte> w(44 + samples * 2);
	memcpy(&w[0], "RIFF", 4);
	WRITE_LE_UINT32(&w[4], (uint32)w.size() - 8);
	memcpy(&w[8], "WAVEfmt ", 8);
	WRITE_LE_UINT32(&w[16], 16);
	WRITE_LE_UINT16(&w[20], 1);
	WRITE_LE_UINT16(&w[22], 1);
	WRITE_LE_UINT32(&w[24], 22050);
	WRITE_LE_UINT32(&w[28], 44100);
	WRITE_LE_UINT16(&w[32], 2);
	WRITE_LE_UINT16(&w[34], 16);
	memcpy(&w[36], "data", 4);
	WRITE_LE_UINT32(&w[40], samples * 2);
	return w;
}

} // End of anonymous namespace

TEST(sound_file_names_and_subtitles) {
	MissionSound::Location where;
	CHECK_EQ(MissionSound::getFileName(0x4006B, "WAV", where), Common::String("RIKE0107.WAV"));
	CHECK(where == MissionSound::kLocationMission);
	CHECK_EQ(MissionSound::getFileName(0x1010035, "TXT", where), Common::String("PICA0053.TXT"));
	CHECK(where == MissionSound::kLocationCommon);
	CHECK_EQ(MissionSound::getFileName(0x121D4D, "WAV", where), Common::String("MUSI7501.WAV"));
	CHECK(where == MissionSound::kLocationMusic);

	CHECK_EQ(MissionSound::cleanSubtitle("RIKER: \\T00FF00\\Hello\\\\ there\r\n"),
	         Common::String("RIKER: Hello\\ there"));
}

TEST(sound_cues_positions_and_events) {
	std::vector<byte> scriptBytes = makeSoundScript();
	Common::MemoryReadStream scriptStream(scriptBytes.data(), (uint32)scriptBytes.size());
	STG::ScriptFile script;
	CHECK(script.load(scriptStream));

	Audio::Mixer mixer(22050);
	mixer.setReady(true);
	std::vector<byte> wav = makeWAV();
	std::vector<Common::String> opened;
	auto opener = [&](const Common::String &name, MissionSound::Location where)
		-> std::unique_ptr<Common::SeekableReadStream> {
		opened.push_back(name);
		if (name == "RIKE0107.TXT") {
			static const char text[] = "RIKER: \\T00FF00\\Make it so.";
			return std::unique_ptr<Common::SeekableReadStream>(
				new Common::MemoryReadStream((const byte *)text, sizeof(text) - 1));
		}
		return std::unique_ptr<Common::SeekableReadStream>(new Common::MemoryReadStream(wav.data(), (uint32)wav.size()));
	};
	MissionSound sound(script, &mixer, opener, nullptr);
	CHECK_EQ(sound.getSoundCount(), 4);
	CHECK_EQ(sound.getCueCount(), 1);

	std::vector<int16> buffer(22050 * 2);
	auto run = [&](uint32 ms, std::vector<MissionSound::Finished> &finished) {
		mixer.mixCallback(buffer.data(), 22050 * ms / 1000);
		MissionSound::Listener listener = { 0, 0, 0, 0 };
		sound.update(ms, listener, finished);
	};

	// The cue: the line, its subtitle, a pause, the line again, then event 46.
	std::vector<MissionSound::Finished> finished;
	sound.playCue(5, 0xE001);
	run(10, finished);
	CHECK_EQ(opened.size(), (size_t)2);
	CHECK_EQ(opened[0], Common::String("RIKE0107.WAV"));
	Common::String text;
	uint32 duration = 0;
	CHECK(sound.takeSubtitle(text, duration));
	CHECK_EQ(text, Common::String("RIKER: Make it so."));
	CHECK_EQ(duration, 2000u);
	for (int i = 0; i < 3; i++)
		run(100, finished);
	CHECK_EQ(opened.size(), (size_t)2); // the first line is over; the pause is not
	for (int i = 0; i < 3; i++)
		run(100, finished);
	CHECK_EQ(opened.size(), (size_t)4); // the second line
	CHECK(finished.empty());
	for (int i = 0; i < 3; i++)
		run(100, finished);
	CHECK_EQ(finished.size(), (size_t)1);
	CHECK_EQ(finished[0].object, 0xE001);

	// A positional sound plays within its radius and is dropped beyond it.
	opened.clear();
	int handle = sound.play(1);
	CHECK(handle > 0);
	run(10, finished);
	CHECK_EQ(opened.size(), (size_t)1);
	opened.clear();
	sound.play(1);
	MissionSound::Listener far = { 100, 0, 0, 0 };
	finished.clear();
	sound.update(10, far, finished);
	CHECK(opened.empty());

	// Fading out stops it.
	CHECK(sound.fadeOut((uint32)handle << 16, 50));
	run(100, finished);
	run(100, finished);
	CHECK(!sound.fadeOut((uint32)handle << 16, 50));
}
