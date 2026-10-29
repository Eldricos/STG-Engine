/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "engines/stg/keymap.h"

#include <cstdlib>
#include <cstring>

namespace STG {

const byte KeyMap::kAwayPanel[kAwayActions] = {
	kAwayAttack, kAwayForward, kAwayLeft, kAwayRight, kAwayBackward, kAwaySlideLeft, kAwaySlideRight,
	kAwayJump, kAwayCrouch, kAwayRun, kAwayLookUp, kAwayLookDown, kAwayLookCenter, kAwayMouseLook
};

const byte KeyMap::kStarshipPanel[kStarshipActions] = {
	kShipPhasers, kShipPhotons, kShipFaster, kShipSlower, kShipLeft, kShipRight, kShipRollLeft, kShipRollRight,
	kShipClimb, kShipDive, kShipHelm + 0, kShipHelm + 1, kShipHelm + 2, kShipHelm + 3, kShipHelm + 4,
	kShipHelm + 5, kShipHelm + 6
};

void KeyMap::clear() {
	memset(_map, kNone, sizeof(_map));
}

KeyMap KeyMap::awayDefaults() {
	// 0x488977.
	KeyMap m;
	m.set(0x26, kAwayForward);    // Up
	m.set(0x28, kAwayBackward);   // Down
	m.set(0x25, kAwayLeft);       // Left
	m.set(0x27, kAwayRight);      // Right
	m.set(0x21, kAwayLookUp);     // Page Up
	m.set(0x22, kAwayLookDown);   // Page Down
	m.set(0x0D, kAwayJump);       // Enter
	m.set(0x11, kAwayCrouch);     // Ctrl
	m.set(0x2D, kAwayAttack);     // Insert
	m.set(0x01, kAwayAttack);     // left button
	m.set(0x02, kAwayMouseLook);  // right button
	m.set(0x24, kAwayLookCenter); // Home
	m.set(0x10, kAwayRun);        // Shift
	m.set(0x6A, kAwaySlideRight); // Num *
	m.set(0x6F, kAwaySlideLeft);  // Num /
	return m;
}

KeyMap KeyMap::starshipDefaults() {
	// 0x488A23.
	KeyMap m;
	m.set(0x26, kShipDive);      // Up
	m.set(0x68, kShipDive);      // Num 8
	m.set(0x28, kShipClimb);     // Down
	m.set(0x62, kShipClimb);     // Num 2
	m.set(0x25, kShipLeft);      // Left
	m.set(0x64, kShipLeft);      // Num 4
	m.set(0x27, kShipRight);     // Right
	m.set(0x66, kShipRight);     // Num 6
	m.set(0x11, kShipRollLeft);  // Ctrl
	m.set(0x60, kShipRollRight); // Num 0
	m.set(0x10, kShipPhasers);   // Shift
	m.set(0x0D, kShipPhotons);   // Enter
	m.set(0x6B, kShipFaster);    // Num +
	m.set(0x6D, kShipSlower);    // Num -
	for (int i = 0; i < 7; i++)
		m.set('1' + i, kShipHelm + i);
	m.set(0x65, kShipHelm + 3); // Num 5: Full Stop
	return m;
}

void KeyMap::findKeys(byte action, int &first, int &second) const {
	// 0x485CF0 keeps the first key found in the low byte until a second
	// shifts it up; the panel shows the low byte first.
	int found[2] = { -1, -1 };
	int count = 0;
	for (int vk = 0; vk < kKeys && count < 2; vk++) {
		if (_map[vk] == action)
			found[count++] = vk;
	}
	first = count == 2 ? found[1] : found[0];
	second = count == 2 ? found[0] : -1;
}

Common::String KeyMap::toString() const {
	Common::String text;
	for (int vk = 0; vk < kKeys; vk++) {
		if (_map[vk] == kNone)
			continue;
		if (!text.empty())
			text += ",";
		text += Common::format("%x:%x", vk, _map[vk]);
	}
	return text;
}

bool KeyMap::fromString(const Common::String &text) {
	KeyMap m;
	for (const Common::String &pair : Common::split(text, ',')) {
		Common::String item = Common::trim(pair);
		if (item.empty())
			continue;
		size_t colon = item.find(':');
		if (colon == Common::String::npos)
			return false;
		char *end;
		long vk = strtol(item.c_str(), &end, 16);
		if (end != item.c_str() + colon || vk < 0 || vk >= kKeys)
			return false;
		long action = strtol(item.c_str() + colon + 1, &end, 16);
		if (*end || end == item.c_str() + colon + 1 || action < 0 || action > 0xFF)
			return false;
		m._map[vk] = (byte)action;
	}
	*this = m;
	return true;
}

bool KeyMap::operator==(const KeyMap &o) const {
	return memcmp(_map, o._map, sizeof(_map)) == 0;
}

int KeyMap::virtualKey(Common::KeyCode key) {
	using namespace Common;
	if (key >= KEYCODE_a && key <= KEYCODE_z)
		return 'A' + (key - KEYCODE_a);
	if (key >= KEYCODE_0 && key <= KEYCODE_9)
		return key;
	if (key >= KEYCODE_KP0 && key <= KEYCODE_KP9)
		return 0x60 + (key - KEYCODE_KP0);
	if (key >= KEYCODE_F1 && key <= KEYCODE_F12)
		return 0x70 + (key - KEYCODE_F1);
	switch (key) {
	case KEYCODE_BACKSPACE:
		return 0x08;
	case KEYCODE_TAB:
		return 0x09;
	case KEYCODE_RETURN:
	case KEYCODE_KP_ENTER:
		return 0x0D;
	case KEYCODE_LSHIFT:
	case KEYCODE_RSHIFT:
		return 0x10;
	case KEYCODE_LCTRL:
	case KEYCODE_RCTRL:
		return 0x11;
	case KEYCODE_LALT:
	case KEYCODE_RALT:
		return 0x12;
	case KEYCODE_PAUSE:
		return 0x13;
	case KEYCODE_CAPSLOCK:
		return 0x14;
	case KEYCODE_ESCAPE:
		return 0x1B;
	case KEYCODE_SPACE:
		return 0x20;
	case KEYCODE_PAGEUP:
		return 0x21;
	case KEYCODE_PAGEDOWN:
		return 0x22;
	case KEYCODE_END:
		return 0x23;
	case KEYCODE_HOME:
		return 0x24;
	case KEYCODE_LEFT:
		return 0x25;
	case KEYCODE_UP:
		return 0x26;
	case KEYCODE_RIGHT:
		return 0x27;
	case KEYCODE_DOWN:
		return 0x28;
	case KEYCODE_PRINT:
		return 0x2C;
	case KEYCODE_INSERT:
		return 0x2D;
	case KEYCODE_DELETE:
		return 0x2E;
	case KEYCODE_KP_MULTIPLY:
		return 0x6A;
	case KEYCODE_KP_PLUS:
		return 0x6B;
	case KEYCODE_KP_MINUS:
		return 0x6D;
	case KEYCODE_KP_PERIOD:
		return 0x6E;
	case KEYCODE_KP_DIVIDE:
		return 0x6F;
	case KEYCODE_NUMLOCK:
		return 0x90;
	case KEYCODE_SCROLLOCK:
		return 0x91;
	case KEYCODE_SEMICOLON:
		return 0xBA;
	case KEYCODE_EQUALS:
		return 0xBB;
	case KEYCODE_COMMA:
		return 0xBC;
	case KEYCODE_MINUS:
		return 0xBD;
	case KEYCODE_PERIOD:
		return 0xBE;
	case KEYCODE_SLASH:
		return 0xBF;
	case KEYCODE_BACKQUOTE:
		return 0xC0;
	case KEYCODE_LEFTBRACKET:
		return 0xDB;
	case KEYCODE_BACKSLASH:
		return 0xDC;
	case KEYCODE_RIGHTBRACKET:
		return 0xDD;
	case KEYCODE_QUOTE:
		return 0xDE;
	default:
		return -1;
	}
}

Common::String KeyMap::keyName(int vk) {
	if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9'))
		return Common::String(1, (char)vk);
	if (vk >= 0x60 && vk <= 0x69)
		return Common::format("num %d", vk - 0x60);
	if (vk >= 0x70 && vk <= 0x7B)
		return Common::format("F%d", vk - 0x6F);
	switch (vk) {
	case 0x01:
		return "mb 1";
	case 0x02:
		return "mb 2";
	case 0x04:
		return "mb 3";
	case 0x08:
		return "bs";
	case 0x09:
		return "tab";
	case 0x0D:
		return "enter";
	case 0x10:
		return "shift";
	case 0x11:
		return "ctrl";
	case 0x12:
		return "alt";
	case 0x13:
		return "pause";
	case 0x20:
		return "space";
	case 0x21:
		return "pgup";
	case 0x22:
		return "pgdn";
	case 0x23:
		return "end";
	case 0x24:
		return "home";
	case 0x25:
		return "left";
	case 0x26:
		return "up";
	case 0x27:
		return "right";
	case 0x28:
		return "down";
	case 0x2D:
		return "ins";
	case 0x2E:
		return "del";
	case 0x6A:
		return "num *";
	case 0x6B:
		return "num +";
	case 0x6D:
		return "num -";
	case 0x6E:
		return "num .";
	case 0x6F:
		return "num /";
	case 0x90:
		return "numlk";
	case 0x91:
		return "scrlk";
	case 0xBA:
		return ";";
	case 0xBB:
		return "=";
	case 0xBC:
		return ",";
	case 0xBD:
		return "-";
	case 0xBE:
		return ".";
	case 0xBF:
		return "/";
	case 0xC0:
		return "`";
	case 0xDB:
		return "[";
	case 0xDC:
		return "\\";
	case 0xDD:
		return "]";
	case 0xDE:
		return "'";
	default:
		return "?!?";
	}
}

} // End of namespace STG
