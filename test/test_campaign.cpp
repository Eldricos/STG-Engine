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
