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
#include "common/md5.h"
#include "engines/detection.h"
#include "engines/engine.h"
#include "engines/metaengine.h"

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
	gameDir.writeFile("ART/PICTURE.BMP", Test::makeBMP8(2, 2, { 1, 1, 1, 1 }, false));

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
	CHECK_EQ(system.getScreen().getPixel(320, 240), 1u);
	CHECK_EQ(system.getScreen().getPixel(8, 100), 0u);
	CHECK_EQ(system.getPalette()[1 * 3 + 1], 128);
	CHECK_EQ(system.getPalette()[254 * 3], 255);

	SearchMan.clear();
	ConfMan.setActiveDomain("");
	g_system = nullptr;
}
