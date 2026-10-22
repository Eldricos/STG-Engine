/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "test/test.h"

#include <vector>

#include "engines/stg/hud.h"

namespace {

struct Sent {
	uint16 object, event;
	std::vector<int16> args;
};

} // End of anonymous namespace

TEST(hud_inventory) {
	std::vector<Sent> sent;
	STG::Hud hud([&](uint16 o, uint16 e, const std::vector<int16> &a) { sent.push_back({ o, e, a }); });
	CHECK_EQ(hud.addItem(0x2001, -1), 1); // the first free slot is 1, not 0
	CHECK_EQ(hud.addItem(0x2002, 0), 0);
	CHECK_EQ(sent.size(), (size_t)2);
	CHECK_EQ(sent[0].event, STG::Hud::kEventAdded);
	// An item already in the slot asked for moves to a free one.
	CHECK_EQ(hud.addItem(0x2003, 1), 1);
	CHECK_EQ(hud.getSlot(2), 0x2001);
	hud.removeItem(0x2001);
	CHECK_EQ(hud.findItem(0x2001), -1);
	for (int i = 0; i < 10; i++)
		hud.addItem(0x2100 + i, -1);
	CHECK_EQ(hud.addItem(0x2200, -1), -1); // full
}

TEST(hud_panels) {
	std::vector<Sent> sent;
	STG::Hud hud([&](uint16 o, uint16 e, const std::vector<int16> &a) { sent.push_back({ o, e, a }); });
	hud.showPanel(0xA004, 12, 0, 10, 10);
	// Without buttons a click is event 45...
	hud.clickPanel(5, 5);
	CHECK_EQ(sent.back().event, STG::Hud::kEventActivate);
	// ...with them, 47 and the button hit, or 0.
	hud.addButton(0xA004, 1, 35, Common::Rect(126, 73, 169, 116), false);
	hud.clickPanel(130, 80);
	CHECK_EQ(sent.back().event, STG::Hud::kEventButton);
	CHECK_EQ(sent.back().args[0], 1);
	hud.clickPanel(5, 5);
	CHECK_EQ(sent.back().args[0], 0);

	// A second panel shows instead; the cycle button goes back.
	hud.showPanel(0xA005, 24, 0, 10, 10);
	CHECK_EQ(hud.getVisiblePanel()->owner, 0xA005);
	hud.nextPanel();
	CHECK_EQ(hud.getVisiblePanel()->owner, 0xA004);

	// Button animation: frames 0-2 twice, then it stays on the last.
	hud.animateButton(0xA004, 1, 0, 2, 2, 100);
	sent.clear();
	hud.update(250, 10, 10);
	CHECK_EQ(hud.getVisiblePanel()->buttons[0].frame, 2);
	hud.update(400, 10, 10);
	CHECK_EQ(hud.getVisiblePanel()->buttons[0].frame, 2);
	CHECK(!hud.getVisiblePanel()->buttons[0].animation.running);
	CHECK(sent.empty());

	// Walking away: event 35 to both, until they are removed.
	hud.update(10, 13, 10);
	CHECK_EQ(sent.size(), (size_t)2);
	CHECK_EQ(sent[0].event, STG::Hud::kEventWalkedAway);
	hud.removePanel(0xA004);
	CHECK_EQ(hud.getVisiblePanel()->owner, 0xA005);
	hud.removePanel(0xA005);
	CHECK(!hud.getVisiblePanel());
}

TEST(hud_effects) {
	std::vector<Sent> sent;
	STG::Hud hud([&](uint16 o, uint16 e, const std::vector<int16> &a) { sent.push_back({ o, e, a }); });
	// 50% red over 100 ms in, 100 held, 100 out, then event 55.
	hud.setEffect(0, 200, 0, 0, 50, 100, 100, 100, 0xE001);
	byte r, g, b;
	CHECK_EQ(hud.getEffectWeight(0, r, g, b), 0);
	hud.update(50, 0, 0);
	CHECK_EQ(hud.getEffectWeight(0, r, g, b), 32);
	CHECK_EQ(r, 200);
	hud.update(100, 0, 0);
	CHECK_EQ(hud.getEffectWeight(0, r, g, b), 64);
	hud.update(100, 0, 0);
	CHECK_EQ(hud.getEffectWeight(0, r, g, b), 32);
	CHECK(sent.empty());
	hud.update(60, 0, 0);
	CHECK_EQ(hud.getEffectWeight(0, r, g, b), 0);
	CHECK_EQ(sent.size(), (size_t)1);
	CHECK_EQ(sent[0].event, STG::Hud::kEventEffectDone);
	CHECK_EQ(sent[0].object, 0xE001);
	// A tint stays.
	hud.setEffect(3, 0, 0, 200, 30, 0, 0, 0, 0);
	hud.update(10000, 0, 0);
	CHECK_EQ(hud.getEffectWeight(3, r, g, b), 38);
}
