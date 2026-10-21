/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "test/test.h"
#include "test/null-system.h"

#include "common/archive.h"
#include "common/config-manager.h"
#include "common/file.h"
#include "common/md5.h"
#include "engines/detection.h"
#include "engines/engine.h"
#include "engines/metaengine.h"
#include "engines/stg/font.h"
#include "engines/stg/overlay.h"
#include "engines/stg/resources.h"
#include "engines/stg/script.h"
#include "engines/stg/sprite.h"

namespace {

const PlainGameDescriptor testGames[] = {
	{ "testgame", "Test Game" },
	{ nullptr, nullptr }
};

/** A fake Windows executable containing the game title in UTF-16, as version resources do. */
std::vector<unsigned char> makeFakeExe() {
	std::vector<unsigned char> exe(0x200, 0);
	exe[0] = 'M';
	exe[1] = 'Z';
	exe[0x3C] = 0x80;
	exe[0x80] = 'P';
	exe[0x81] = 'E';
	const char *title = "Star Trek Generations";
	for (size_t i = 0; title[i]; i++)
		exe[0x100 + i * 2] = (unsigned char)title[i];
	return exe;
}

Common::Event keyEvent(Common::KeyCode key) {
	Common::Event event;
	event.type = Common::EVENT_KEYDOWN;
	event.kbd.keycode = key;
	return event;
}

} // End of anonymous namespace

TEST(detection_table_matching) {
	Test::TempDir dir;
	dir.writeFile("GAME.EXE", "executable");
	dir.writeFile("data/RES.DAT", "resources");

	Common::String exeMD5 = Common::computeMD5("executable", 10);
	static Common::String md5Storage;
	md5Storage = exeMD5;

	const GameDescription table[] = {
		{ "testgame", "CD",
		  { { "game.exe", md5Storage.c_str(), 10 }, { "DATA/res.dat", nullptr, -1 }, { nullptr, nullptr, 0 } },
		  kLangEnglish, kPlatformWindows, GF_CD },
		{ "testgame", "Demo",
		  { { "demo.exe", nullptr, -1 }, { nullptr, nullptr, 0 } },
		  kLangEnglish, kPlatformWindows, GF_DEMO },
		{ nullptr, nullptr, { { nullptr, nullptr, 0 } }, kLangUnknown, kPlatformUnknown, GF_NONE }
	};

	DetectedGames games = detectGamesFromTable(dir.node(), table, "test", testGames);
	CHECK_EQ(games.size(), (size_t)1);
	if (!games.empty()) {
		CHECK_EQ(games[0].gameId, Common::String("testgame"));
		CHECK_EQ(games[0].extra, Common::String("CD"));
		CHECK(!games[0].unknownVariant);
		CHECK(games[0].engineData == &table[0]);
		CHECK_EQ(games[0].getFullDescription(), Common::String("Test Game (CD/Windows/English)"));
		CHECK_EQ(games[0].getPreferredTarget(), Common::String("testgame-win"));
	}

	// Same files, different contents: reported as an unknown variant.
	Test::TempDir other;
	other.writeFile("GAME.EXE", "different!");
	other.writeFile("DATA/RES.DAT", "resources");
	games = detectGamesFromTable(other.node(), table, "test", testGames);
	CHECK_EQ(games.size(), (size_t)1);
	if (!games.empty()) {
		CHECK(games[0].unknownVariant);
		CHECK_EQ(games[0].matchedFiles.size(), (size_t)2);
	}

	Test::TempDir empty;
	CHECK(detectGamesFromTable(empty.node(), table, "test", testGames).empty());
}

TEST(stg_registered_and_fallback_detection) {
	const MetaEngine *stg = findMetaEngine("STG");
	CHECK(stg != nullptr);
	if (!stg)
		return;
	CHECK_EQ(stg->getSupportedGames().size(), (size_t)1);

	Test::TempDir dir;
	dir.writeFile("README.TXT", "nothing here");
	CHECK(stg->detectGames(dir.node()).empty());

	dir.writeFile("Trek.exe", makeFakeExe());
	DetectedGames games = stg->detectGames(dir.node());
	CHECK_EQ(games.size(), (size_t)1);
	if (!games.empty()) {
		CHECK_EQ(games[0].gameId, Common::String("stg"));
		CHECK(games[0].unknownVariant);
		CHECK_EQ(games[0].platform, kPlatformWindows);
	}
}

TEST(stg_engine_runs_headless) {
	const MetaEngine *stg = findMetaEngine("stg");
	CHECK(stg != nullptr);
	if (!stg)
		return;

	Test::TempDir gameDir, saveDir;
	gameDir.writeFile("TREK.EXE", makeFakeExe());
	gameDir.writeFile("PICTURE.BMP", Test::makeBMP8(2, 2, { 1, 1, 1, 1 }, false));

	DetectedGames games = stg->detectGames(gameDir.node());
	CHECK(!games.empty());
	if (games.empty())
		return;

	OSystem_Null system(saveDir.node().getPath());
	g_system = &system;
	system.initBackend();
	ConfMan.setActiveDomain("stg-test");

	SearchMan.clear();
	SearchMan.addDirectory("game", games[0].path, 0, 4);

	system.scheduleEvent(2, keyEvent(Common::KEYCODE_F5));
	Common::Event click;
	click.type = Common::EVENT_LBUTTONDOWN;
	click.mouse = Common::Point(100, 100);
	system.scheduleEvent(3, click);
	system.scheduleEvent(5, keyEvent(Common::KEYCODE_F7));
	system.scheduleEvent(7, keyEvent(Common::KEYCODE_RIGHT));
	system.scheduleEvent(10, keyEvent(Common::KEYCODE_ESCAPE));

	std::unique_ptr<Engine> engine;
	Common::Error err = stg->createInstance(&system, games[0], engine);
	CHECK(!err.failed());
	if (engine)
		err = engine->run();
	CHECK(!err.failed());
	engine.reset();

	CHECK_EQ(system.getWidth(), 640);
	CHECK_EQ(system.getHeight(), 480);
	CHECK(system.getFrameCount() >= 10);
	CHECK(system.isCursorSet());
	CHECK(system.isMouseVisible());
	CHECK(system.getSavefileManager()->exists("stg-test.s00"));

	// The image from the game directory is shown centred, with its palette
	// loaded over the viewer's own (which keeps orange in entry 254).
	// A 2x2 image is enlarged 3x and centred above the 60 pixel text panel.
	CHECK_EQ(system.getScreen().getPixel(320, 210), 1u);
	CHECK_EQ(system.getScreen().getPixel(8, 100), 0u);
	CHECK_EQ(system.getPalette()[1 * 3 + 1], 128);
	CHECK_EQ(system.getPalette()[254 * 3], 255);

	SearchMan.clear();
	ConfMan.setActiveDomain("");
	g_system = nullptr;
}

TEST(stg_two_cd_search_paths) {
	Test::TempDir install, cd1, cd2;
	const char *installed[] = { "COMMON.DAT", "OPTIONS.DAT", "AMARGOS1.DAT", "AMARGOS2.DAT", "ANTILIOS.DAT",
	                            "ARVADA1.DAT", "BERSUS.DAT", "ENTERP.DAT", "EPSION.DAT", "GALORN1.DAT",
	                            "GALORN2.DAT", "HALEE1.DAT", "QURASH.DAT", "VERID1.DAT" };
	for (const char *name : installed)
		install.writeFile(std::string("DATA/") + name, "installed");
	install.writeFile("DATA/COMMON.DAT", std::vector<unsigned char>(Test::kTestZip, Test::kTestZip + Test::kTestZipSize));
	install.writeFile("sgens.exe", makeFakeExe());

	const char *disc1[] = { "COMBAT.DAT", "STELLAR.DAT", "MUSIC.1", "MOVIE/MOV_01.AVI", "MOVIE/MOV_02.AVI",
	                        "MOVIE/MOV_03.AVI", "MOVIE/MOV_05.AVI", "MOVIE/MOV_07.AVI", "MOVIE/MOV_08.AVI",
	                        "MOVIE/MOV_18.AVI" };
	const char *disc2[] = { "MUSIC.2", "MOVIE/MOV_06.AVI", "MOVIE/MOV_09.AVI", "MOVIE/MOV_10.AVI",
	                        "MOVIE/MOV_13.AVI", "MOVIE/MOV_14.AVI", "MOVIE/MOV_15.AVI", "MOVIE/MOV_16.AVI" };
	for (const char *name : disc1)
		cd1.writeFile(std::string("GENS/") + name, "cd1");
	for (const char *name : disc2)
		cd2.writeFile(std::string("GENS/") + name, "cd2");
	// Also on the disc, but the installed copy must win.
	cd1.writeFile("GENS/COMMON.DAT", "cd1");

	STG::ResourceManager resources;
	SearchMan.clear();

	resources.addSearchPaths(install.node(), cd1.node().getPath(), "");
	CHECK(!resources.checkDataFiles());

	resources.addSearchPaths(install.node(), cd1.node().getPath(), cd2.node().getPath());
	CHECK(resources.checkDataFiles());

	Common::File f;
	CHECK(f.open("options.dat"));
	CHECK_EQ(f.readFixedString(9), Common::String("installed"));

	// COMMON.DAT is a ZIP: it is opened, and its members become visible by name.
	resources.openArchives();
	CHECK(resources.getArchive("common.dat") != nullptr);
	CHECK(resources.getArchive("VERID1.DAT") == nullptr); // not a ZIP in this test
	CHECK(f.open("short.txt"));
	CHECK_EQ(f.readFixedString(12), Common::String("abcabcabcabc"));
	CHECK(f.open("movie/mov_16.avi"));
	CHECK_EQ(f.readFixedString(3), Common::String("cd2"));
	CHECK(f.open("SGENS.EXE"));

	SearchMan.clear();
}

TEST(stg_font_format) {
	// Built to the layout seen in COMMON.DAT:FONTS/ACTION.FNT: header, then
	// glyphs of code, size (w*h+2), width, height and pixels.
	Common::MemoryWriteStreamDynamic glyphs;
	auto addGlyph = [&](uint16 code, byte w, byte h, std::vector<byte> pixels) {
		glyphs.writeUint16LE(code);
		glyphs.writeUint16LE((uint16)(w * h + 2));
		glyphs.writeByte(w);
		glyphs.writeByte(h);
		glyphs.write(pixels.data(), (uint32)pixels.size());
	};
	addGlyph(' ', 2, 3, { 0, 0, 0, 0, 0, 0 });
	addGlyph('A', 2, 2, { 5, 0, 0, 6 });

	Common::MemoryWriteStreamDynamic file;
	file.writeUint32BE(MKTAG('F', 'O', 'N', 'T'));
	file.writeUint32LE(8 + glyphs.size());
	file.writeUint16LE(2);
	file.writeByte(0); // transparent
	file.writeByte(3); // height
	file.writeUint32LE(0);
	file.write(glyphs.getData().data(), glyphs.size());

	Common::MemoryReadStream stream(file.getData().data(), file.size());
	STG::Font font;
	CHECK(font.load(stream));
	CHECK_EQ(font.getGlyphCount(), 2);
	CHECK_EQ(font.getHeight(), 3);
	CHECK_EQ(font.getStringWidth("A A"), 6);
	CHECK_EQ(font.getCharWidth('a'), 2); // falls back to upper case

	Graphics::Surface s;
	s.create(8, 4, Graphics::PixelFormat::createFormatCLUT8());
	s.fillRect(Common::Rect(8, 4), 9);
	font.drawString(s, " A", 0, 0);
	CHECK_EQ(s.getPixel(0, 0), 9u); // space is transparent
	CHECK_EQ(s.getPixel(2, 0), 5u);
	CHECK_EQ(s.getPixel(3, 0), 9u);
	CHECK_EQ(s.getPixel(3, 1), 6u);
	byte remap[256];
	for (int i = 0; i < 256; i++)
		remap[i] = (byte)(i + 100);
	font.drawString(s, "A", 4, 0, remap);
	CHECK_EQ(s.getPixel(4, 0), 105u); // glyph colour 5 remapped
	CHECK_EQ(s.getPixel(5, 1), 106u);
	CHECK_EQ(s.getPixel(5, 0), 9u);   // transparent stays untouched

	CHECK_EQ(font.wordWrap("A A A A", 7).size(), (size_t)2);

	// A glyph whose size field disagrees with its dimensions is rejected.
	std::vector<byte> bad = file.getData();
	bad[20] = 99;
	Common::MemoryReadStream badStream(bad.data(), (uint32)bad.size());
	CHECK(!font.load(badStream));
}

TEST(stg_subtitles) {
	Test::TempDir dir;
	dir.writeFile("SOUNDS/RIKE0107.WAV", "RIFF");
	dir.writeFile("SOUNDS/rike0107.txt", "RIKER: \"CAPTAIN, WE'VE GOT\r\nA SERIOUS SITUATION.\"\r\n");
	SearchMan.clear();
	SearchMan.addDirectory("data", dir.node(), 0, 2);

	STG::ResourceManager resources;
	CHECK_EQ(resources.loadSubtitle({ "", "SOUNDS/RIKE0107.WAV" }),
	         Common::String("RIKER: \"CAPTAIN, WE'VE GOT A SERIOUS SITUATION.\""));
	CHECK(resources.loadSubtitle({ "", "SOUNDS/MISC0001.WAV" }).empty());
	SearchMan.clear();
}

TEST(stg_overlay_format) {
	// Width 4, 4 rows = two frames of 2 rows. Rows are int16 runs: negative
	// skips transparent pixels, positive is followed by pixels padded to even.
	std::vector<std::vector<int>> rows = {
		{ -4 },               // all transparent
		{ -1, 2, 7, 8, -1 },  // .78.
		{ 3, 1, 2, 3, -1 },   // 123. (odd run, padded)
		{ 4, 9, 9, 9, 9 },    // 9999
	};
	Common::MemoryWriteStreamDynamic data;
	std::vector<uint32> offsets;
	for (const std::vector<int> &row : rows) {
		offsets.push_back(data.size());
		for (size_t i = 0; i < row.size();) {
			int n = row[i++];
			data.writeUint16LE((uint16)(int16)n);
			if (n > 0) {
				for (int k = 0; k < n; k++)
					data.writeByte((byte)row[i++]);
				if (n & 1)
					data.writeByte(0);
			}
		}
	}

	Common::MemoryWriteStreamDynamic file;
	file.write("OVERLAY\0", 8);
	file.writeUint32LE(4);
	file.writeUint32LE((uint32)rows.size());
	for (uint32 offset : offsets)
		file.writeUint32LE(offset);
	file.write(data.getData().data(), data.size());

	Common::MemoryReadStream stream(file.getData().data(), file.size());
	STG::Overlay overlay;
	CHECK(overlay.load(stream));
	CHECK_EQ(overlay.getWidth(), 4);
	CHECK_EQ(overlay.getRowCount(), 4);
	CHECK(!overlay.isOpaque(0, 0));
	CHECK(!overlay.isOpaque(0, 1));
	CHECK(overlay.isOpaque(1, 1));
	CHECK_EQ(overlay.getSurface().getPixel(2, 1), 8u);
	CHECK_EQ(overlay.getSurface().getPixel(2, 2), 3u);
	CHECK(!overlay.isOpaque(3, 2));

	// Frame 1 (rows 2-3) drawn over a filled surface keeps the transparent pixel.
	Graphics::Surface dst;
	dst.create(4, 2, Graphics::PixelFormat::createFormatCLUT8());
	dst.fillRect(Common::Rect(4, 2), 5);
	overlay.drawFrame(dst, 1, 2, 0, 0);
	CHECK_EQ(dst.getPixel(0, 0), 1u);
	CHECK_EQ(dst.getPixel(3, 0), 5u);
	CHECK_EQ(dst.getPixel(3, 1), 9u);

	// A row whose runs overflow the width is rejected.
	std::vector<byte> bad = file.getData();
	bad[16 + 4 * 4] = 0x7F; // first row: skip becomes a 127 pixel run
	Common::MemoryReadStream badStream(bad.data(), (uint32)bad.size());
	CHECK(!overlay.load(badStream));
}

TEST(stg_sprite_format) {
	// Two frames: 2 columns x 3 rows and 1 column x 2 rows, stored
	// column-major, bottom row first, 0xFF transparent, then 3 bytes of
	// per-column tables per column.
	Common::MemoryWriteStreamDynamic file;
	file.writeUint32LE(2); // version
	file.writeUint32LE(2); // frames
	file.writeUint32LE(0);
	file.writeUint32LE(3); // sequences
	file.writeUint32LE(0);
	file.writeUint16LE(64);
	file.writeUint16LE(128);
	const byte sequences[] = { 2, 0, 1, 0, 1, 1 }; // [0,1], [], [1]
	file.write(sequences, sizeof(sequences));

	file.writeUint32LE(4 << 8); // world width 4.0
	file.writeUint32LE(2);
	file.writeUint32LE(3);
	const byte frame0[] = { 1, 2, 0xFF, 4, 5, 6 }; // column 0 bottom-up, column 1 bottom-up
	file.write(frame0, sizeof(frame0));
	const byte tables0[6] = {};
	file.write(tables0, sizeof(tables0));

	file.writeUint32LE(2 << 8);
	file.writeUint32LE(1);
	file.writeUint32LE(2);
	const byte frame1[] = { 7, 8, 0, 0, 0 };
	file.write(frame1, sizeof(frame1));

	Common::MemoryReadStream stream(file.getData().data(), file.size());
	STG::SpriteSet sprites;
	CHECK(sprites.load(stream));
	CHECK_EQ(sprites.getFrameCount(), 2);
	CHECK_EQ(sprites.getSequences().size(), (size_t)3);
	CHECK(sprites.getSequences()[1].empty());
	CHECK_EQ(sprites.getMaxWidth(), 2);
	CHECK_EQ(sprites.getMaxHeight(), 3);

	const Graphics::Surface &s0 = sprites.getFrame(0).surface;
	CHECK_EQ(sprites.getFrame(0).worldWidth, 4.0);
	CHECK_EQ(s0.getPixel(0, 2), 1u); // bottom row first
	CHECK_EQ(s0.getPixel(0, 1), 2u);
	CHECK_EQ(s0.getPixel(0, 0), 0xFFu);
	CHECK_EQ(s0.getPixel(1, 0), 6u);
	CHECK_EQ(sprites.getFrame(1).surface.getPixel(0, 0), 8u);

	// Truncated data is rejected.
	Common::MemoryReadStream truncated(file.getData().data(), file.size() - 8);
	CHECK(!sprites.load(truncated));
}

TEST(stg_script_container) {
	// One object with one state whose enter block does "push 5, push 7, SET_GLOBAL".
	const uint16 code[] = { 1, 9, 0x8000, 2, 0xFFFF, 0x0002, 5, 0x0002, 7, 0x0019, 0, 0x3000, 0x2000, 0x1000 };
	const byte sections[6][2] = { { 1, 2 }, { 3, 4 }, { 5, 6 }, { 7, 8 }, { 9, 10 }, { 11, 12 } };
	const char names[] = "PHASER.BMP\0KIRKHIT.OVR\0";

	Common::MemoryWriteStreamDynamic body;
	body.writeUint32LE(sizeof(code));
	for (int i = 0; i < 6; i++)
		body.writeUint32LE(2);
	for (uint16 word : code)
		body.writeUint16LE(word);
	for (int i = 0; i < 6; i++)
		body.write(sections[i], 2);
	body.write(names, sizeof(names) - 1);

	Common::MemoryWriteStreamDynamic file;
	file.writeUint32BE(MKTAG('S', 'G', 'E', 'N'));
	file.writeUint16LE(22);
	file.writeUint16LE(2);
	file.writeUint32LE(0); // file size - 12, patched below
	file.write(body.getData().data(), body.size());
	std::vector<byte> bytes = file.getData();
	WRITE_LE_UINT32(&bytes[8], (uint32)bytes.size() - 12);

	Common::MemoryReadStream stream(bytes.data(), (uint32)bytes.size());
	STG::ScriptFile script;
	CHECK(script.load(stream));
	CHECK_EQ(script.getVersion(), 22);
	CHECK_EQ(script.getCode().size(), (size_t)14);
	CHECK_EQ(script.getCode()[9], 0x19);
	CHECK_EQ(script.getObjects().size(), (size_t)1);
	CHECK_EQ(script.getSection(5)[1], 12);
	CHECK_EQ(script.getResourceNames().size(), (size_t)2);
	CHECK_EQ(script.getResourceNames()[1], Common::String("KIRKHIT.OVR"));

	// Sizes that don't match the file are rejected.
	WRITE_LE_UINT32(&bytes[8], 1234);
	Common::MemoryReadStream bad(bytes.data(), (uint32)bytes.size());
	CHECK(!script.load(bad));
}
