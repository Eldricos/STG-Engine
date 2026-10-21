/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_KEYBOARD_H
#define COMMON_KEYBOARD_H

#include "common/types.h"

namespace Common {

/**
 * Platform independent key codes. Printable keys use their lower-case ASCII
 * value; the backend maps host key codes onto these.
 */
enum KeyCode {
	KEYCODE_INVALID = 0,

	KEYCODE_BACKSPACE = 8,
	KEYCODE_TAB = 9,
	KEYCODE_RETURN = 13,
	KEYCODE_ESCAPE = 27,
	KEYCODE_SPACE = 32,
	KEYCODE_QUOTE = 39,
	KEYCODE_COMMA = 44,
	KEYCODE_MINUS = 45,
	KEYCODE_PERIOD = 46,
	KEYCODE_SLASH = 47,
	KEYCODE_0 = 48,
	KEYCODE_1, KEYCODE_2, KEYCODE_3, KEYCODE_4, KEYCODE_5, KEYCODE_6, KEYCODE_7, KEYCODE_8, KEYCODE_9,
	KEYCODE_SEMICOLON = 59,
	KEYCODE_EQUALS = 61,
	KEYCODE_LEFTBRACKET = 91,
	KEYCODE_BACKSLASH = 92,
	KEYCODE_RIGHTBRACKET = 93,
	KEYCODE_BACKQUOTE = 96,
	KEYCODE_a = 97,
	KEYCODE_b, KEYCODE_c, KEYCODE_d, KEYCODE_e, KEYCODE_f, KEYCODE_g, KEYCODE_h, KEYCODE_i,
	KEYCODE_j, KEYCODE_k, KEYCODE_l, KEYCODE_m, KEYCODE_n, KEYCODE_o, KEYCODE_p, KEYCODE_q,
	KEYCODE_r, KEYCODE_s, KEYCODE_t, KEYCODE_u, KEYCODE_v, KEYCODE_w, KEYCODE_x, KEYCODE_y,
	KEYCODE_z,
	KEYCODE_DELETE = 127,

	KEYCODE_KP0 = 256,
	KEYCODE_KP1, KEYCODE_KP2, KEYCODE_KP3, KEYCODE_KP4, KEYCODE_KP5, KEYCODE_KP6, KEYCODE_KP7,
	KEYCODE_KP8, KEYCODE_KP9,
	KEYCODE_KP_PERIOD, KEYCODE_KP_DIVIDE, KEYCODE_KP_MULTIPLY, KEYCODE_KP_MINUS, KEYCODE_KP_PLUS,
	KEYCODE_KP_ENTER,

	KEYCODE_UP = 273,
	KEYCODE_DOWN, KEYCODE_RIGHT, KEYCODE_LEFT, KEYCODE_INSERT, KEYCODE_HOME, KEYCODE_END,
	KEYCODE_PAGEUP, KEYCODE_PAGEDOWN,

	KEYCODE_F1 = 282,
	KEYCODE_F2, KEYCODE_F3, KEYCODE_F4, KEYCODE_F5, KEYCODE_F6, KEYCODE_F7, KEYCODE_F8,
	KEYCODE_F9, KEYCODE_F10, KEYCODE_F11, KEYCODE_F12,

	KEYCODE_NUMLOCK = 300,
	KEYCODE_CAPSLOCK, KEYCODE_SCROLLOCK, KEYCODE_RSHIFT, KEYCODE_LSHIFT, KEYCODE_RCTRL,
	KEYCODE_LCTRL, KEYCODE_RALT, KEYCODE_LALT,

	KEYCODE_PRINT = 316,
	KEYCODE_PAUSE = 19
};

enum KeyModifier {
	KBD_CTRL = 1 << 0,
	KBD_ALT = 1 << 1,
	KBD_SHIFT = 1 << 2,
	KBD_META = 1 << 3,
	KBD_NUM = 1 << 4,
	KBD_CAPS = 1 << 5
};

struct KeyState {
	KeyCode keycode = KEYCODE_INVALID;
	/** The character this key press produces, or 0 if none. */
	uint16 ascii = 0;
	/** Bitmask of KeyModifier values. */
	byte flags = 0;

	KeyState() {}
	KeyState(KeyCode kc, uint16 asc = 0, byte f = 0) : keycode(kc), ascii(asc), flags(f) {}

	bool hasFlags(byte f) const { return (flags & f) == f; }
};

} // End of namespace Common

#endif
