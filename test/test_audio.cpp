/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "test/test.h"

#include <cstdlib>

#include "common/audio/mixer.h"
#include "common/audio/raw.h"
#include "common/audio/wave.h"
#include "common/stream.h"

static std::vector<byte> makePCM16(int frames, int16 value) {
	std::vector<byte> data(frames * 2);
	for (int i = 0; i < frames; i++)
		WRITE_LE_UINT16(&data[i * 2], (uint16)value);
	return data;
}

TEST(raw_stream_formats) {
	// 8-bit unsigned: 0x80 is silence, 0xFF near full scale.
	std::vector<byte> pcm8 = { 0x80, 0xFF, 0x00 };
	auto s8 = Audio::makeRawStream(std::move(pcm8), 11025, Audio::FLAG_UNSIGNED);
	int16 buf[4] = {};
	CHECK_EQ(s8->readBuffer(buf, 4), 3);
	CHECK_EQ(buf[0], 0);
	CHECK_EQ(buf[1], 127 * 256);
	CHECK_EQ(buf[2], -128 * 256);
	CHECK(s8->endOfData());
	CHECK(s8->rewind());
	CHECK(!s8->endOfData());

	// A trailing odd byte of a 16-bit stereo stream is dropped.
	std::vector<byte> pcm16 = { 0x34, 0x12, 0xCC, 0xED, 0x00 };
	auto s16 = Audio::makeRawStream(std::move(pcm16), 22050,
	                                Audio::FLAG_16BITS | Audio::FLAG_LITTLE_ENDIAN | Audio::FLAG_STEREO);
	CHECK(s16->isStereo());
	CHECK_EQ(s16->readBuffer(buf, 4), 2);
	CHECK_EQ(buf[0], 0x1234);
	CHECK_EQ(buf[1], (int16)0xEDCC);
}

TEST(looping_stream) {
	auto stream = Audio::makeRawStream(makePCM16(10, 1), 22050, Audio::FLAG_16BITS | Audio::FLAG_LITTLE_ENDIAN);
	auto looped = Audio::makeLoopingAudioStream(std::move(stream), 3);
	int16 buf[64];
	CHECK_EQ(looped->readBuffer(buf, 64), 30);
	CHECK(looped->endOfData());
}

TEST(wave_parsing) {
	Common::MemoryWriteStreamDynamic w;
	std::vector<byte> pcm = makePCM16(4, 1000);
	w.writeUint32BE(MKTAG('R', 'I', 'F', 'F'));
	w.writeUint32LE(4 + 8 + 16 + 8 + 2 + 8 + (uint32)pcm.size());
	w.writeUint32BE(MKTAG('W', 'A', 'V', 'E'));
	w.writeUint32BE(MKTAG('f', 'm', 't', ' '));
	w.writeUint32LE(16);
	w.writeUint16LE(1);     // PCM
	w.writeUint16LE(1);     // mono
	w.writeUint32LE(22050); // rate
	w.writeUint32LE(44100); // bytes/sec
	w.writeUint16LE(2);     // block align
	w.writeUint16LE(16);    // bits
	// An odd-sized unknown chunk, which must be skipped with its pad byte.
	w.writeUint32BE(MKTAG('j', 'u', 'n', 'k'));
	w.writeUint32LE(1);
	w.writeByte(0xAA);
	w.writeByte(0);
	w.writeUint32BE(MKTAG('d', 'a', 't', 'a'));
	w.writeUint32LE((uint32)pcm.size());
	w.write(pcm.data(), (uint32)pcm.size());

	Common::MemoryReadStream r(w.getData().data(), w.size());
	Audio::WAVInfo info;
	CHECK(Audio::loadWAVFromStream(r, info));
	CHECK_EQ(info.sampleRate, 22050u);
	CHECK_EQ(info.dataSize, 8u);

	r.seek(0);
	auto stream = Audio::makeWAVStream(r);
	CHECK(stream != nullptr);
	if (stream) {
		int16 buf[8];
		CHECK_EQ(stream->readBuffer(buf, 8), 4);
		CHECK_EQ(buf[3], 1000);
		CHECK_EQ(stream->getRate(), 22050);
	}

	const byte notWave[] = "RIFF\0\0\0\0AVI ";
	Common::MemoryReadStream bad(notWave, sizeof(notWave));
	CHECK(!Audio::loadWAVFromStream(bad, info));
}

TEST(mixer_volume_and_resampling) {
	Audio::Mixer mixer(22050);
	int16 out[200];

	// Not ready: silence, and the sound stays queued.
	Audio::SoundHandle handle;
	mixer.playStream(Audio::Mixer::kSFXSoundType, &handle,
	                 Audio::makeRawStream(makePCM16(1000, 8000), 11025, Audio::FLAG_16BITS | Audio::FLAG_LITTLE_ENDIAN));
	mixer.mixCallback(out, 100);
	CHECK_EQ(out[0], 0);
	CHECK(mixer.isSoundHandleActive(handle));

	mixer.setReady(true);
	mixer.mixCallback(out, 100);
	// Mono is duplicated to both channels at roughly unity gain.
	CHECK(abs(out[0] - 8000 * 255 / 256) < 64);
	CHECK_EQ(out[0], out[1]);

	// Halving the category volume halves the output.
	mixer.setVolumeForSoundType(Audio::Mixer::kSFXSoundType, 128);
	mixer.mixCallback(out, 100);
	CHECK(abs(out[0] - 4000) < 64);

	// Full left balance silences the right channel.
	mixer.setChannelBalance(handle, -127);
	mixer.mixCallback(out, 100);
	CHECK(out[0] != 0);
	CHECK_EQ(out[1], 0);

	// 1000 frames at 11025 Hz last 2000 output frames at 22050 Hz.
	for (int i = 0; i < 20; i++)
		mixer.mixCallback(out, 100);
	CHECK(!mixer.isSoundHandleActive(handle));
}

TEST(mixer_clipping_and_ids) {
	Audio::Mixer mixer(22050);
	mixer.setReady(true);
	for (int i = 0; i < 4; i++)
		mixer.playStream(Audio::Mixer::kPlainSoundType, nullptr,
		                 Audio::makeRawStream(makePCM16(100, 30000), 22050, Audio::FLAG_16BITS | Audio::FLAG_LITTLE_ENDIAN));
	int16 out[20];
	mixer.mixCallback(out, 10);
	CHECK_EQ(out[0], 32767);

	mixer.stopAll();
	Audio::SoundHandle first, second;
	mixer.playStream(Audio::Mixer::kMusicSoundType, &first,
	                 Audio::makeRawStream(makePCM16(100, 1), 22050, Audio::FLAG_16BITS | Audio::FLAG_LITTLE_ENDIAN), 7);
	mixer.playStream(Audio::Mixer::kMusicSoundType, &second,
	                 Audio::makeRawStream(makePCM16(100, 1), 22050, Audio::FLAG_16BITS | Audio::FLAG_LITTLE_ENDIAN), 7);
	CHECK(!mixer.isSoundHandleActive(first));
	CHECK(mixer.isSoundHandleActive(second));
	CHECK(mixer.isSoundTypeActive(Audio::Mixer::kMusicSoundType));
	mixer.stopID(7);
	CHECK(!mixer.isSoundHandleActive(second));
}
