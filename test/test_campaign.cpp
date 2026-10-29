/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "test/test.h"

#include "common/serializer.h"
#include "engines/stg/campaign.h"
#include "engines/stg/optionsmenu.h"

namespace {

// A made-up script in the shape of the game's.
const char *const kScript =
	"[Start]\r\n"
	"Intro\r\n"
	"Jump NewGame\r\n"
	"\r\n"
	"[NewGame]\r\n"
	"Movie 1\r\n"
	"SetFlag 3 TRUE\t\t\t;a comment\r\n"
	"ShowStellar\r\n"
	"Jump Planet\r\n"
	"[Planet]\r\n"
	"OnTransport PlanetPlay\r\n"
	"Jump Missed\r\n"
	"[PlanetPlay]\r\n"
	"ActionGame 4 PlanetWon PlanetLost\r\n"
	"[PlanetLost]\r\n"
	"IfNotFlag 3 Jump Missed\r\n"
	"IfFlag 3 IfActionGamePlayed 4 Movie 7\r\n"
	"Destroy Planet\r\n"
	"IfSunsDestroyed 1 Jump Fight\r\n"
	"Jump Missed\r\n"
	"[PlanetWon]\r\n"
	"AutoSave\r\n"
	"JumpRandom Missed Fight\r\n"
	"[Fight]\r\n"
	"OnTactical FightPlay\r\n"
	"Jump Won\r\n"
	"[FightPlay]\r\n"
	"SpaceCombat 2 Won Missed\r\n"
	"[Won]\r\n"
	"Movie 15\r\n"
	"EndGame\r\n"
	"[Missed]\r\n"
	"ScrollText 1\r\n"
	"Options\r\n";

STG::Campaign::Action::Type next(STG::Campaign &c, int *number = nullptr) {
	STG::Campaign::Action a = c.next();
	if (number)
		*number = a.number;
	return a.type;
}

} // End of anonymous namespace

TEST(campaign_runs_the_script) {
	typedef STG::Campaign::Action A;
	STG::Campaign c;
	CHECK(!c.load("[Other]\nIntro\n"));
	CHECK(c.load(kScript));
	c.start("Start");
	int n = 0;
	CHECK_EQ(next(c), A::kIntro);
	CHECK_EQ(next(c, &n), A::kMovie);
	CHECK_EQ(n, 1);
	CHECK_EQ(next(c, &n), A::kActionGame);
	CHECK_EQ(n, 4);
	// Nothing runs while the mission does.
	CHECK_EQ(next(c), A::kNone);
	CHECK(c.isRunning());

	// Losing: the flags and the conditions, then a destroyed sun.
	c.missionEnded(false);
	CHECK_EQ(next(c, &n), A::kMovie);
	CHECK_EQ(n, 7);
	// No fleet is ever engaged: the section goes on.
	CHECK_EQ(next(c, &n), A::kMovie);
	CHECK_EQ(n, 15);
	CHECK_EQ(next(c), A::kEndGame);
	CHECK(!c.isRunning());
	CHECK_EQ(next(c), A::kNone);

	// Winning, then the second random choice.
	c.setRandom([](int count) { return count - 1; });
	c.start("NewGame");
	CHECK_EQ(next(c), A::kMovie);
	CHECK_EQ(next(c), A::kActionGame);
	c.missionEnded(true);
	CHECK_EQ(next(c), A::kAutoSave);

	// A save made here carries on the same way.
	Common::MemoryWriteStreamDynamic out;
	Common::Serializer save(nullptr, &out);
	c.sync(save);
	STG::Campaign d;
	d.load(kScript);
	d.setRandom([](int count) { return count - 1; });
	Common::MemoryReadStream in(out.getData().data(), out.size());
	Common::Serializer load(&in, nullptr);
	d.sync(load);
	CHECK(!load.err());
	CHECK_EQ(d.getSection(), Common::String("planetwon"));
	CHECK_EQ(next(d, &n), A::kMovie);
	CHECK_EQ(n, 15);

	c.setRandom([](int) { return 0; });
	CHECK_EQ(next(c, &n), A::kScrollText);
	CHECK_EQ(n, 1);
	CHECK_EQ(next(c), A::kOptions);
	CHECK(!c.isRunning());
}

TEST(options_menu_panels) {
	typedef STG::OptionsMenu M;
	STG::ResourceManager resources;
	M menu(resources); // no pictures: the logic works without them
	std::vector<int> sounds;
	menu.setSoundCallback([&](int id) { sounds.push_back(id); });
	STG::GameSettings settings;
	menu.open(settings, false, false, true);
	CHECK_EQ(sounds.back(), 1000);
	auto click = [&](int x, int y) {
		menu.mouseDown(x, y);
		menu.mouseUp(x, y);
	};

	// Nothing to return to, nothing to save.
	click(400, 400);
	click(150, 70);
	CHECK_EQ(menu.getPanel(), M::kPanelMain);
	CHECK_EQ(menu.takeResult(), M::kNone);

	// Settings: the difficulty steps round; Accept keeps it.
	click(150, 160);
	CHECK_EQ(menu.getPanel(), M::kPanelSettings);
	click(227 + 190, 29 + 195);
	click(227 + 110 + 212, 29 + 130); // past the slider's end: no change
	click(227 + 106 + 70, 29 + 130);  // a third of the way along
	click(400, 400);
	CHECK_EQ(menu.getPanel(), M::kPanelMain);
	CHECK(menu.takeSettingsChanged());
	CHECK_EQ(menu.getSettings().difficulty, 2);
	CHECK_EQ(menu.getSettings().effectsVolume, 9);

	// Load: an empty slot can't be picked.
	std::vector<M::Slot> slots(M::kAutoSaveSlot + 1);
	slots[1] = { true, "Before the door", "10/22 14:05" };
	menu.setSlots(slots);
	click(150, 40);
	click(300, 29 + 33 + 10);
	CHECK_EQ(sounds.back(), 1002);
	click(300, 29 + 33 + 30 + 10);
	click(400, 400);
	CHECK_EQ(menu.takeResult(), M::kLoad);
	CHECK_EQ(menu.getSlot(), 1);

	// Quit asks first; Cancel goes back.
	click(150, 130);
	CHECK_EQ(menu.getPanel(), M::kPanelQuit);
	menu.keyDown(Common::KeyState(Common::KEYCODE_ESCAPE));
	CHECK_EQ(sounds.back(), 2000);
	CHECK_EQ(menu.takeResult(), M::kNone);
	click(150, 100);
	menu.keyDown(Common::KeyState(Common::KEYCODE_RETURN));
	CHECK_EQ(menu.takeResult(), M::kNewGame);

	// In a game: Return, and Save with a name typed in.
	menu.open(settings, true, true, false);
	click(150, 70);
	click(300, 29 + 33 + 10);
	menu.keyDown(Common::KeyState(Common::KEYCODE_a, 'a'));
	menu.keyDown(Common::KeyState(Common::KEYCODE_b, 'b'));
	menu.keyDown(Common::KeyState(Common::KEYCODE_BACKSPACE));
	click(400, 400);
	CHECK_EQ(menu.takeResult(), M::kSave);
	CHECK_EQ(menu.getSlot(), 0);
	CHECK_EQ(menu.getSaveName(), Common::String("a"));
	click(400, 400);
	CHECK_EQ(menu.takeResult(), M::kReturn);
}

TEST(key_maps) {
	using STG::KeyMap;
	KeyMap away = KeyMap::awayDefaults();
	CHECK_EQ(away.get(0x26), (int)KeyMap::kAwayForward);
	CHECK_EQ(away.get(0x02), (int)KeyMap::kAwayMouseLook);
	CHECK_EQ(away.get('W'), (int)KeyMap::kNone);
	// Two keys: the panel shows the higher one first.
	int first, second;
	away.findKeys(KeyMap::kAwayAttack, first, second);
	CHECK_EQ(first, 0x2D);
	CHECK_EQ(second, 0x01);
	away.findKeys(KeyMap::kAwayRun, first, second);
	CHECK_EQ(first, 0x10);
	CHECK_EQ(second, -1);
	KeyMap ship = KeyMap::starshipDefaults();
	CHECK_EQ(ship.get(0x65), KeyMap::kShipHelm + 3);
	CHECK_EQ(ship.get('7'), KeyMap::kShipHelm + 6);

	// Through the configuration and back.
	KeyMap read;
	CHECK(read.fromString(away.toString()));
	CHECK(read == away);
	CHECK(!read.fromString("26:1,zz"));
	CHECK(read == away);
	CHECK(read.fromString(""));
	CHECK_EQ(read.get(0x26), (int)KeyMap::kNone);

	CHECK_EQ(KeyMap::virtualKey(Common::KEYCODE_w), 'W');
	CHECK_EQ(KeyMap::virtualKey(Common::KEYCODE_KP_PLUS), 0x6B);
	CHECK_EQ(KeyMap::virtualKey(Common::KEYCODE_RCTRL), 0x11);
	CHECK_EQ(KeyMap::keyName('W'), Common::String("W"));
	CHECK_EQ(KeyMap::keyName(0x64), Common::String("num 4"));
	CHECK_EQ(KeyMap::keyName(0x7A), Common::String("F11"));
	CHECK_EQ(KeyMap::keyName(0x02), Common::String("mb 2"));
	CHECK_EQ(KeyMap::keyName(0x1B), Common::String("?!?"));
}

TEST(options_menu_controls) {
	using STG::KeyMap;
	typedef STG::OptionsMenu M;
	STG::ResourceManager resources;
	M menu(resources);
	STG::GameSettings settings;
	menu.open(settings, false, false, true);
	auto click = [&](int x, int y) {
		menu.mouseDown(x, y);
		menu.mouseUp(x, y);
	};
	// The middle of a key cell: two to an action, down two columns.
	auto cell = [&](int action, int which) {
		int x = 227 + (action < 10 ? 84 : 252) + 34 * which + 16, y = 29 + 37 + 30 * (action % 10) + 13;
		click(x, y);
	};
	auto press = [&](Common::KeyCode key) { menu.keyDown(Common::KeyState(key)); };
	click(150, 195);
	CHECK_EQ(menu.getPanel(), M::kPanelControls);

	// W for Forward in place of Up.
	cell(1, 0);
	press(Common::KEYCODE_w);
	// Then W for Jump: it leaves Forward.
	cell(7, 1);
	press(Common::KEYCODE_F5); // not the function keys
	press(Common::KEYCODE_w);
	// Mouse Look takes only the left and right buttons; Escape gives up.
	cell(13, 0);
	press(Common::KEYCODE_k);
	menu.mouseDown(10, 10, 4);
	press(Common::KEYCODE_ESCAPE);
	CHECK_EQ(menu.getPanel(), M::kPanelControls);
	click(400, 400);
	CHECK(menu.takeSettingsChanged());
	const KeyMap &keys = menu.getSettings().awayKeys;
	CHECK_EQ(keys.get('W'), (int)KeyMap::kAwayJump);
	CHECK_EQ(keys.get(0x26), (int)KeyMap::kNone);
	CHECK_EQ(keys.get(0x0D), (int)KeyMap::kAwayJump);
	CHECK_EQ(keys.get('K'), (int)KeyMap::kNone);
	CHECK_EQ(keys.get(0x02), (int)KeyMap::kAwayMouseLook);

	// The starship page; Cancel keeps the old keys.
	click(150, 195);
	click(227 + 200, 29 + 10);
	cell(0, 0);
	press(Common::KEYCODE_f);
	menu.keyDown(Common::KeyState(Common::KEYCODE_ESCAPE));
	CHECK_EQ(menu.getPanel(), M::kPanelMain);
	CHECK(!menu.takeSettingsChanged());
	CHECK_EQ(menu.getSettings().starshipKeys.get('F'), (int)KeyMap::kNone);
	click(150, 195);
	click(227 + 200, 29 + 10);
	cell(0, 1);
	menu.mouseDown(10, 10, 2);
	click(400, 400);
	CHECK_EQ(menu.getSettings().starshipKeys.get(0x02), (int)KeyMap::kShipPhasers);
	CHECK_EQ(menu.getSettings().starshipKeys.get(0x10), (int)KeyMap::kShipPhasers); // the other cell keeps Shift
}
