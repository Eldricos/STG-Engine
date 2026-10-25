/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "test/test.h"

#include <cstring>

#include "common/archive.h"
#include "common/config-manager.h"
#include "common/endian.h"
#include "common/file.h"
#include "common/md5.h"
#include "common/random.h"
#include "common/rect.h"
#include "common/savefile.h"
#include "common/stream.h"
#include "common/str.h"

TEST(endian_roundtrip) {
	byte buf[4];
	WRITE_LE_UINT32(buf, 0x12345678);
	CHECK_EQ(buf[0], 0x78);
	CHECK_EQ(READ_LE_UINT32(buf), 0x12345678u);
	CHECK_EQ(READ_BE_UINT32(buf), 0x78563412u);
	WRITE_BE_UINT16(buf, 0xABCD);
	CHECK_EQ(READ_LE_UINT16(buf), 0xCDABu);
	CHECK_EQ(MKTAG('R', 'I', 'F', 'F'), 0x52494646u);

	const byte wide[8] = { 1, 2, 3, 4, 5, 6, 7, 0x80 };
	CHECK(READ_LE_UINT64(wide) == 0x8007060504030201ull);
}

TEST(string_helpers) {
	CHECK_EQ(Common::toLower("GAME.EXE"), Common::String("game.exe"));
	CHECK(Common::equalsIgnoreCase("Data/Foo.BMP", "data/foo.bmp"));
	CHECK(Common::hasSuffixIgnoreCase("INTRO.WAV", ".wav"));
	CHECK(!Common::hasSuffixIgnoreCase("WAV", ".wav"));
	CHECK_EQ(Common::trim("  key = value \r"), Common::String("key = value"));
	CHECK_EQ(Common::split("a,b,,c", ',').size(), (size_t)4);
	CHECK_EQ(Common::format("%s-%02d", "stg", 3), Common::String("stg-03"));

	CHECK(Common::matchString("SOUND01.WAV", "*.wav"));
	CHECK(Common::matchString("SOUND01.WAV", "sound??.*"));
	CHECK(!Common::matchString("SOUND01.WAV", "*.bmp"));
	CHECK(Common::matchString("", "*"));
	CHECK(!Common::matchString("abc", "ab"));
}

TEST(rect_clip) {
	Common::Rect r(-10, -10, 50, 30);
	r.clip(Common::Rect(40, 20));
	CHECK_EQ(r.left, 0);
	CHECK_EQ(r.top, 0);
	CHECK_EQ(r.right, 40);
	CHECK_EQ(r.bottom, 20);
	CHECK(r.contains(39, 19));
	CHECK(!r.contains(40, 19));

	Common::Rect outside(100, 100, 120, 120);
	outside.clip(Common::Rect(40, 20));
	CHECK(outside.isEmpty());
}

TEST(memory_stream) {
	const byte data[] = { 0x01, 0x02, 0x03, 0x04, 'h', 'i', 0, 'x', 'L', '1', '\r', '\n', 'L', '2' };
	Common::MemoryReadStream s(data, sizeof(data));
	CHECK_EQ(s.size(), (int64)sizeof(data));
	CHECK_EQ(s.readUint16LE(), 0x0201u);
	CHECK_EQ(s.readUint16BE(), 0x0304u);
	CHECK_EQ(s.readFixedString(4), Common::String("hi"));
	CHECK_EQ(s.readLine(), Common::String("L1"));
	CHECK_EQ(s.readLine(), Common::String("L2"));
	CHECK(s.eos());
	CHECK(s.seek(2));
	CHECK(!s.eos());
	CHECK_EQ(s.readByte(), 0x03);
	CHECK(!s.seek(100));
}

TEST(sub_stream) {
	std::vector<byte> data;
	for (int i = 0; i < 100; i++)
		data.push_back((byte)i);
	auto parent = std::make_shared<Common::MemoryReadStream>(std::move(data));
	Common::SubReadStream sub(parent, 10, 20);
	CHECK_EQ(sub.size(), (int64)10);
	CHECK_EQ(sub.readByte(), 10);
	CHECK(sub.seek(-1, Common::kSeekEnd));
	CHECK_EQ(sub.readByte(), 19);
	byte b;
	CHECK_EQ(sub.read(&b, 1), 0u);
	CHECK(sub.eos());
}

TEST(md5_vectors) {
	CHECK_EQ(Common::computeMD5("", 0), Common::String("d41d8cd98f00b204e9800998ecf8427e"));
	CHECK_EQ(Common::computeMD5("abc", 3), Common::String("900150983cd24fb0d6963f7d28e17f72"));
	const char *fox = "The quick brown fox jumps over the lazy dog";
	CHECK_EQ(Common::computeMD5(fox, (uint32)strlen(fox)), Common::String("9e107d9d372bb6826bd81d3542a419d6"));

	// The stream version honours the length limit.
	Common::MemoryReadStream s((const byte *)fox, (uint32)strlen(fox));
	CHECK_EQ(Common::computeStreamMD5(s, 3), Common::computeMD5("The", 3));
}

TEST(random_deterministic) {
	Common::RandomSource a(1234), b(1234);
	for (int i = 0; i < 100; i++) {
		uint32 v = a.getRandomNumber(9);
		CHECK_EQ(v, b.getRandomNumber(9));
		CHECK(v <= 9);
	}
	uint32 r = a.getRandomNumberRng(5, 7);
	CHECK(r >= 5 && r <= 7);
}

TEST(fs_case_insensitive_lookup) {
	Test::TempDir dir;
	dir.writeFile("GAME.EXE", "MZ");
	dir.writeFile("DATA/Intro.WAV", "RIFF");

	Common::FSNode child = dir.node().getChild("game.exe");
	CHECK(child.exists());
	CHECK_EQ(child.getName(), Common::String("GAME.EXE"));

	Common::FSDirectory archive(dir.node(), 2);
	CHECK(archive.hasFile("data/intro.wav"));
	CHECK(archive.hasFile("DATA\\INTRO.WAV"));
	CHECK(!archive.hasFile("intro.wav"));

	Common::ArchiveMemberList wavs;
	CHECK_EQ(archive.listMatchingMembers(wavs, "*.wav"), 1);

	// Depth 1 does not descend into DATA.
	Common::FSDirectory shallow(dir.node(), 1);
	CHECK(!shallow.hasFile("data/intro.wav"));
}

TEST(searchset_priority) {
	Test::TempDir low, high;
	low.writeFile("A.TXT", "low");
	low.writeFile("ONLYLOW.TXT", "x");
	high.writeFile("a.txt", "high");

	Common::SearchSet set;
	set.addDirectory("low", low.node(), 0);
	set.addDirectory("high", high.node(), 10);

	Common::File f;
	CHECK(f.open("A.TXT", set));
	CHECK_EQ(f.readFixedString(4), Common::String("high"));
	CHECK(set.hasFile("onlylow.txt"));

	Common::ArchiveMemberList all;
	set.listMembers(all);
	CHECK_EQ(all.size(), (size_t)2);

	set.remove("high");
	CHECK(f.open("A.TXT", set));
	CHECK_EQ(f.readFixedString(3), Common::String("low"));
}

TEST(config_manager_layers) {
	Test::TempDir dir;
	dir.writeFile("stg.ini",
	              "# comment\n"
	              "[stg-engine]\n"
	              "music_volume=100\n"
	              "fullscreen=true\n"
	              "\n"
	              "[stg-win]\n"
	              "music_volume=50\n"
	              "path=/games/stg\n");

	Common::ConfigManager cfg;
	CHECK(cfg.loadDefaultConfigFile(dir.node().getChild("stg.ini")));
	cfg.registerDefault("sfx_volume", 192);

	CHECK_EQ(cfg.getInt("music_volume"), 100);
	cfg.setActiveDomain("stg-win");
	CHECK_EQ(cfg.getInt("music_volume"), 50);
	CHECK(cfg.getBool("fullscreen"));
	CHECK_EQ(cfg.getInt("sfx_volume"), 192);
	cfg.setTransient("music_volume", "10");
	CHECK_EQ(cfg.getInt("music_volume"), 10);

	CHECK_EQ(cfg.getGameDomains().size(), (size_t)1);
	cfg.set("description", "Test");
	CHECK(cfg.flushToDisk());

	Common::ConfigManager reloaded;
	CHECK(reloaded.loadFromFile(dir.node().getChild("stg.ini")));
	CHECK_EQ(reloaded.get("description", "stg-win"), Common::String("Test"));
	CHECK_EQ(reloaded.get("path", "stg-win"), Common::String("/games/stg"));
	// Transient values are never written.
	CHECK_EQ(reloaded.get("music_volume", "stg-win"), Common::String("50"));
}

TEST(savefile_manager) {
	Test::TempDir dir;
	Common::SaveFileManager saves(dir.node().getChild("saves"));
	{
		std::unique_ptr<Common::OutSaveFile> out = saves.openForSaving("stg.s00");
		CHECK(out != nullptr);
		out->writeUint32LE(0xDEADBEEF);
		out->flush();
	}
	CHECK(saves.exists("stg.s00"));
	CHECK_EQ(saves.listSavefiles("stg.s*").size(), (size_t)1);
	std::unique_ptr<Common::InSaveFile> in = saves.openForLoading("stg.s00");
	CHECK(in != nullptr);
	CHECK_EQ(in->readUint32LE(), 0xDEADBEEFu);
	in.reset();
	CHECK(saves.removeSavefile("stg.s00"));
	CHECK(!saves.exists("stg.s00"));
}
