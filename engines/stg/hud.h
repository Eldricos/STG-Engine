/* STG-Engine - a reimplementation of the Star Trek Generations engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STG_HUD_H
#define STG_HUD_H

#include <functional>
#include <map>
#include <vector>

#include "common/rect.h"
#include "common/types.h"

namespace STG {

/**
 * What the scripts put on the away-team console around the 3D view, and
 * how it reacts: the inventory, close-up panels and their buttons, the
 * body picture, automap markers and screen effects. Drawing is the
 * viewer's (see HudScreen); this keeps the state and sends the events.
 *
 * Images are script resources: indices into GAME.BIN's resource names.
 */
class Hud {
public:
	/** Sends an event to a script object. */
	typedef std::function<void(uint16 object, uint16 event, const std::vector<int16> &args)> EventSink;

	enum Event {
		kEventAdded = 43,      ///< an item went into the inventory
		kEventActivate = 45,   ///< an item or panel was used (clicked)
		kEventButton = 47,     ///< a panel was clicked: the button number, or 0
		kEventEffectDone = 55, ///< a screen effect finished
		kEventPanelDone = 60,  ///< a panel's animation finished
		kEventDescribe = 62,   ///< right click
		kEventDropped = 63,    ///< an item was dragged out of the inventory
		kEventWalkedAway = 35  ///< the player left a panel's object behind
	};

	// --- Inventory (0x446DD0): 12 slots, 3 across and 4 down; slot 0 is
	// only filled on purpose.
	static const int kSlots = 12;

	explicit Hud(const EventSink &send);

	/** Put an item in a slot, or the first free one of 1-11 for -1. Returns the slot, or -1 if full. */
	int addItem(uint16 id, int slot);
	void removeItem(uint16 id);
	uint16 getSlot(int slot) const { return _slots[slot]; }
	/** The slot an item is in, or -1. */
	int findItem(uint16 id) const;
	/** Move an item to another slot (dragging), swapping with what is there. */
	void moveItem(int from, int to);

	// --- Close-up panels (0x6C..0x6F, 0x9D..0xA1, 0x8F, 0x90): one per
	// object, shown one at a time in the console's 211 x 148 viewer.
	static const int kPanelWidth = 211;
	static const int kPanelHeight = 148;

	struct Animation {
		bool running = false;
		int first = 0, last = 0;
		/** Frames still to show; 0 runs forever. */
		int remaining = 0;
		uint32 ms = 100, elapsed = 0;
	};

	struct Button {
		int number = 0;
		int16 image = -1;
		int frame = 0;
		/** Relative to the panel. */
		Common::Rect rect;
		/** Colour-keyed (0xA0); palette index 0xFE is see-through. */
		bool keyed = false;
		/** Invisible: only reports clicks (0x8F). */
		bool hotspot = false;
		Animation animation;
	};

	struct Panel {
		uint16 owner = 0;
		int16 image = -1;
		int frame = 0;
		/** Where the player was when it opened. */
		double x = 0, y = 0;
		std::vector<Button> buttons;
		Animation animation;
		bool notifyWhenDone = false;
	};

	/** PANEL_SHOW_IMAGE: open (or reuse) the object's panel and show it. */
	void showPanel(uint16 owner, int16 image, int frame, double playerX, double playerY);
	/** NATIVE_6D: animate the panel's image; event 60 at the end unless ms is 0. */
	void animatePanel(uint16 owner, int first, int last, int loops, uint32 ms);
	void removePanel(uint16 owner);
	/** The visible panel, or null. */
	const Panel *getVisiblePanel() const;
	/** The first panel's owner (NATIVE_6F), or 0. */
	uint16 getFirstPanelOwner() const { return _panels.empty() ? 0 : _panels.front().owner; }
	int getPanelCount() const { return (int)_panels.size(); }
	/** The cycle button: show the next panel. */
	void nextPanel();

	void addButton(uint16 owner, int number, int16 image, const Common::Rect &rect, bool hotspot);
	void setButton(uint16 owner, int number, int16 image, int frame);
	void animateButton(uint16 owner, int number, int first, int last, int loops, uint32 ms);
	void setButtonKeyed(uint16 owner, int number, bool keyed);
	void removeButton(uint16 owner, int number);

	/** A click at (x, y) in the visible panel: event 45, or 47 with the button hit. */
	void clickPanel(int x, int y);
	/** A right click on the visible panel: event 62. */
	void describePanel();
	/** Space: event 45 to the visible panel's object. */
	void usePanel();

	// --- Body picture (SET_BODY_IMAGE): -1 is the mission's BODY.BMP.
	void setBodyImage(int16 image, int frame) {
		_bodyImage = image;
		_bodyFrame = frame;
	}
	int16 getBodyImage() const { return _bodyImage; }
	int getBodyFrame() const { return _bodyFrame; }

	// --- Automap.
	enum MarkerKind {
		kMarkerLife,
		kMarkerEnergy,
		kMarkerTrilithium
	};
	struct Marker {
		MarkerKind kind;
		int style;
	};
	void setMarker(uint16 id, MarkerKind kind, int style) { _markers[id] = { kind, style }; }
	void removeMarker(uint16 id, MarkerKind kind);
	const std::map<uint16, Marker> &getMarkers() const { return _markers; }
	/** Height bands relative to the player (MAP_SET_LEVELS): low/high pairs. */
	void setMapLevels(const double levels[6]);
	const double *getMapLevels() const { return _levels; }

	// --- Screen effects (0x43199A): four colour slots blended over the
	// palette, slot 0 on top. Times of 0 make a slot stay; 0 percent clears it.
	static const int kEffectSlots = 4;
	struct Effect {
		bool active = false;
		byte r = 0, g = 0, b = 0;
		int percent = 0;
		uint32 fadeIn = 0, hold = 0, fadeOut = 0, elapsed = 0;
		uint16 notify = 0;
	};
	void setEffect(int slot, byte r, byte g, byte b, int percent, uint32 fadeIn, uint32 hold, uint32 fadeOut,
	               uint16 notify);
	/** A slot's colour and weight now, 0-128 (0 = no effect). */
	int getEffectWeight(int slot, byte &r, byte &g, byte &b) const;

	/** VIEW_SHAKE: 0 off, 1 and 2 jitter by 4 and 8 pixels, 4 a single jolt. */
	void setShake(int mode);
	int getShakeMode() const { return _shake; }
	/** The view's offset this frame, in pixels. */
	void getShakeOffset(int &dx, int &dy);
	/** VIEW_OVERLAY: the underwater ripple. */
	void setRipple(bool on) { _ripple = on; }
	bool hasRipple() const { return _ripple; }

	/**
	 * Advance by ms: animations, effects (event 55), and panels whose
	 * object the player has walked away from (event 35, 2 units).
	 */
	void update(uint32 ms, double playerX, double playerY);

private:
	Panel *findPanel(uint16 owner);
	Button *findButton(uint16 owner, int number);
	/** Step an animation; returns true when it has just ended. */
	static bool runAnimation(Animation &animation, int &frame, uint32 ms);

	EventSink _send;
	uint16 _slots[kSlots] = {};
	std::vector<Panel> _panels;
	int _visible = -1;
	int16 _bodyImage = -1;
	int _bodyFrame = 0;
	std::map<uint16, Marker> _markers;
	double _levels[6] = { -1e9, -6, -6, 6, 6, 1e9 };
	Effect _effects[kEffectSlots];
	int _shake = 0;
	double _jolt = 0;
	uint32 _seed = 1;
	bool _ripple = false;
};

} // End of namespace STG

#endif
