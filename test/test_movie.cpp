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

#include "engines/stg/movie.h"

namespace {

typedef std::vector<byte> Bytes;

void be(Bytes &b, uint32 v, int n) {
	for (int i = n - 1; i >= 0; i--)
		b.push_back((byte)(v >> (8 * i)));
}

void le(Bytes &b, uint32 v, int n) {
	for (int i = 0; i < n; i++)
		b.push_back((byte)(v >> (8 * i)));
}

Bytes chunk(int id, const Bytes &body) {
	Bytes b;
	b.push_back((byte)id);
	be(b, (uint32)body.size() + 4, 3);
	b.insert(b.end(), body.begin(), body.end());
	return b;
}

/** An 8 x 4 Cinepak frame of one strip holding the given chunks. */
Bytes frame(const Bytes &chunks, bool intra) {
	Bytes strip;
	strip.push_back(intra ? 0x10 : 0x11);
	be(strip, 12 + (uint32)chunks.size(), 3);
	be(strip, 0, 2); // y1
	be(strip, 0, 2); // x1
	be(strip, 4, 2); // y2
	be(strip, 8, 2); // x2
	strip.insert(strip.end(), chunks.begin(), chunks.end());
	Bytes f;
	f.push_back(0);
	be(f, 10 + (uint32)strip.size(), 3);
	be(f, 8, 2);
	be(f, 4, 2);
	be(f, 1, 2);
	f.insert(f.end(), strip.begin(), strip.end());
	return f;
}

Bytes firstFrame() {
	Bytes chunks;
	Bytes v1 = { 10, 20, 30, 40, 0, 0, 100, 100, 100, 100, 10, (byte)-5 };
	Bytes v4 = { 200, 201, 202, 203, 0, 0 };
	Bytes image;
	be(image, 0x40000000, 4); // block 0 V1, block 1 V4
	image.insert(image.end(), { 0, 0, 0, 0, 0 });
	for (const Bytes &c : { chunk(0x22, v1), chunk(0x20, v4), chunk(0x30, image) })
		chunks.insert(chunks.end(), c.begin(), c.end());
	return frame(chunks, true);
}

Bytes secondFrame() {
	Bytes image;
	be(image, 0x80000000, 4); // block 0 coded as V1, block 1 skipped
	image.push_back(1);
	return frame(chunk(0x31, image), false);
}

uint32 grey(int y) { return 0xFF000000u | (uint32)y << 16 | (uint32)y << 8 | (uint32)y; }

} // End of anonymous namespace

TEST(cinepak_blocks) {
	STG::CinepakDecoder decoder(8, 4);
	Bytes f = firstFrame();
	CHECK(decoder.decode(f.data(), (uint32)f.size()));
	const std::vector<uint32> &p = decoder.getPicture();
	// V1: each luma sample covers 2 x 2 pixels.
	CHECK_EQ(p[0], grey(10));
	CHECK_EQ(p[1 * 8 + 1], grey(10));
	CHECK_EQ(p[2], grey(20));
	CHECK_EQ(p[2 * 8], grey(30));
	CHECK_EQ(p[3 * 8 + 3], grey(40));
	// V4: four 2 x 2 cells.
	CHECK_EQ(p[4], grey(200));
	CHECK_EQ(p[5], grey(201));
	CHECK_EQ(p[8 + 4], grey(202));
	CHECK_EQ(p[8 + 5], grey(203));
	CHECK_EQ(p[6], grey(200));

	// An inter frame redraws the coded block only, from the kept codebook.
	Bytes g = secondFrame();
	CHECK(decoder.decode(g.data(), (uint32)g.size()));
	CHECK_EQ(p[0], 0xFF000000u | 90u << 16 | 100u << 8 | 120u);
	CHECK_EQ(p[5], grey(201));
}

TEST(movie_avi) {
	Bytes video1 = firstFrame(), video2 = secondFrame();
	auto tag = [](Bytes &b, const char *t) { b.insert(b.end(), t, t + 4); };
	auto riffChunk = [&](Bytes &b, const char *t, const Bytes &body) {
		tag(b, t);
		le(b, (uint32)body.size(), 4);
		b.insert(b.end(), body.begin(), body.end());
		if (body.size() & 1)
			b.push_back(0);
	};
	auto list = [&](const char *type, const Bytes &body) {
		Bytes b;
		tag(b, type);
		b.insert(b.end(), body.begin(), body.end());
		return b;
	};

	Bytes avih, vstrh, vstrf, astrh, astrf, hdrl, vstrl, astrl, movi, avi;
	le(avih, 50000, 4);
	avih.resize(56);
	tag(vstrh, "vids");
	tag(vstrh, "cvid");
	vstrh.resize(56);
	le(vstrf, 40, 4);
	le(vstrf, 8, 4);
	le(vstrf, 4, 4);
	vstrf.resize(40);
	tag(astrh, "auds");
	astrh.resize(56);
	le(astrf, 1, 2);
	le(astrf, 2, 2);
	le(astrf, 22050, 4);
	le(astrf, 88200, 4);
	le(astrf, 4, 2);
	le(astrf, 16, 2);
	riffChunk(vstrl, "strh", vstrh);
	riffChunk(vstrl, "strf", vstrf);
	riffChunk(astrl, "strh", astrh);
	riffChunk(astrl, "strf", astrf);
	riffChunk(hdrl, "avih", avih);
	riffChunk(hdrl, "LIST", list("strl", vstrl));
	riffChunk(hdrl, "LIST", list("strl", astrl));
	riffChunk(movi, "00db", video1);
	riffChunk(movi, "01wb", Bytes { 1, 2, 3, 4 });
	riffChunk(movi, "00id", video2);
	riffChunk(movi, "01wb", Bytes { 5, 6, 7, 8 });
	riffChunk(avi, "LIST", list("hdrl", hdrl));
	riffChunk(avi, "LIST", list("movi", movi));
	Bytes file;
	riffChunk(file, "RIFF", list("AVI ", avi));

	STG::Movie movie;
	CHECK(movie.load(std::unique_ptr<Common::SeekableReadStream>(
		new Common::MemoryReadStream(file.data(), (uint32)file.size()))));
	CHECK_EQ(movie.getWidth(), 8);
	CHECK_EQ(movie.getHeight(), 4);
	CHECK_EQ(movie.getFrameCount(), 2);
	CHECK_EQ(movie.getFrameTime(), 50000u);
	CHECK_EQ(movie.getAudioRate(), 22050);
	CHECK_EQ(movie.getAudioChannels(), 2);
	CHECK_EQ(movie.getAudio().size(), (size_t)8);
	CHECK_EQ(movie.getAudio()[4], 5);
	CHECK(movie.nextFrame());
	CHECK_EQ(movie.getPicture()[0], grey(10));
	CHECK(movie.nextFrame());
	CHECK_EQ(movie.getPicture()[0], 0xFF000000u | 90u << 16 | 100u << 8 | 120u);
	CHECK(!movie.nextFrame());
}
